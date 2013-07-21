#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <signal.h>
#include <float.h>
#include <stdlib.h>
#include <set>
#include <tcl.h>
#include "underwatersensor/uw_mac/underwaterchannel.h"
#include "agent.h"
#include "tclcl.h"
#include "ip.h"
#include "config.h"
#include "packet.h"
#include "trace.h"
#include "random.h"
#include "classifier.h"
#include "node.h"
#include "vectorbasedforward.h"
#include "arp.h"
#include "mac.h"
#include "ll.h"
#include "dsr/path.h"
#include "god.h"
#include  "underwatersensor/uw_mac/underwaterpropagation.h"
#include  "underwatersensor/uw_common/underwatersensornode.h"

int hdr_uwvb::offset_;
//#define USE_MODIFIED_VBF
static const double EPSINON = 0.0000001;
static const double NEXTHOP_OMIT_TIMEOUT = 5.0;

static class UWVBHeaderClass: public PacketHeaderClass {
public:
    UWVBHeaderClass():PacketHeaderClass("PacketHeader/UWVB",sizeof(hdr_uwvb))
    {
        bind_offset(&hdr_uwvb::offset_);
    }
} class_uwvbhdr;

void UWPkt_Hash_Table::reset(){
    vbf_neighborhood *hashPtr;
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch searchPtr;

    entryPtr = Tcl_FirstHashEntry(&htable, &searchPtr);
    while (entryPtr != NULL) {
        hashPtr = (vbf_neighborhood *)Tcl_GetHashValue(entryPtr);
        delete hashPtr;
        Tcl_DeleteHashEntry(entryPtr);
        entryPtr = Tcl_NextHashEntry(&searchPtr);
    }
}

vbf_neighborhood* UWPkt_Hash_Table::GetHash(ns_addr_t sender_id,
        unsigned int pk_num){
    unsigned int key[3];

    key[0] = sender_id.addr_;
    key[1] = sender_id.port_;
    key[2] = pk_num;

    Tcl_HashEntry *entryPtr = Tcl_FindHashEntry(&htable, (char *)key);

    if (entryPtr == NULL )
        return NULL;

    return (vbf_neighborhood *)Tcl_GetHashValue(entryPtr);
}

void UWPkt_Hash_Table::put_in_hash(hdr_uwvb * vbh){
    Tcl_HashEntry *entryPtr;
    vbf_neighborhood* hashPtr;
    unsigned int key[3];
    int newPtr;

    key[0]=(vbh->sender_id).addr_;
    key[1]=(vbh->sender_id).port_;
    key[2]=vbh->pk_num;


    int  k=key[2]-window_size;
    if (k>0)
    {
        for (int i=0;i<k;i++)
        {
            key[2]=i;
            entryPtr=Tcl_FindHashEntry(&htable, (char *)key);
            if (entryPtr)
            {
                hashPtr=(vbf_neighborhood*)Tcl_GetHashValue(entryPtr);
                delete hashPtr;
                Tcl_DeleteHashEntry(entryPtr);
            }
        }
    }

    key[2]=vbh->pk_num;
    entryPtr = Tcl_CreateHashEntry(&htable, (char *)key, &newPtr);
    if (!newPtr) {
        hashPtr=GetHash(vbh->sender_id,vbh->pk_num);
        int m=hashPtr->number;
        if (m<MAX_NEIGHBOR) {
            hashPtr->number++;
            hashPtr->neighbor[m].x=0;
            hashPtr->neighbor[m].y=0;
            hashPtr->neighbor[m].z=0;
        }
        return;
    }
    hashPtr=new vbf_neighborhood[1];
    hashPtr[0].number=1;
    hashPtr[0].neighbor[0].x=0;
    hashPtr[0].neighbor[0].y=0;
    hashPtr[0].neighbor[0].z=0;
    Tcl_SetHashValue(entryPtr, hashPtr);

}

void UWPkt_Hash_Table::put_in_hash(hdr_uwvb * vbh, position* p){
    Tcl_HashEntry *entryPtr;
    vbf_neighborhood* hashPtr;
    unsigned int key[3];
    int newPtr;

    key[0]=(vbh->sender_id).addr_;
    key[1]=(vbh->sender_id).port_;
    key[2]=vbh->pk_num;
    int  k=key[2]-window_size;
    if (k>0)
    {
        for (int i=0;i<k;i++) {
            key[2]=i;
            entryPtr=Tcl_FindHashEntry(&htable, (char *)key);
            if (entryPtr)
            {
                hashPtr=(vbf_neighborhood*)Tcl_GetHashValue(entryPtr);
                delete hashPtr;
                Tcl_DeleteHashEntry(entryPtr);
            }

        }
    }
    key[2]=vbh->pk_num;
    entryPtr = Tcl_CreateHashEntry(&htable, (char *)key, &newPtr);
    if (!newPtr)
    {

        hashPtr=GetHash(vbh->sender_id,vbh->pk_num);
        int m=hashPtr->number;
        printf("hash_table: this is not old item, there are %d item inside\n",m);
        if (m<MAX_NEIGHBOR) {
            hashPtr->number++;
            hashPtr->neighbor[m].x=p->x;
            hashPtr->neighbor[m].y=p->y;
            hashPtr->neighbor[m].z=p->z;
        }
        return;
    }
    hashPtr=new vbf_neighborhood[1];
    hashPtr[0].number=1;
    hashPtr[0].neighbor[0].x=p->x;
    hashPtr[0].neighbor[0].y=p->y;
    hashPtr[0].neighbor[0].z=p->z;
    Tcl_SetHashValue(entryPtr, hashPtr);

}

void UWData_Hash_Table::reset(){
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch searchPtr;

    entryPtr = Tcl_FirstHashEntry(&htable, &searchPtr);
    while (entryPtr != NULL) {
        Tcl_DeleteHashEntry(entryPtr);
        entryPtr = Tcl_NextHashEntry(&searchPtr);
    }
}

Tcl_HashEntry  *UWData_Hash_Table::GetHash(int *attr){
    return Tcl_FindHashEntry(&htable, (char *)attr);
}

void UWData_Hash_Table::PutInHash(int *attr){
    int newPtr;
    Tcl_HashEntry* entryPtr=Tcl_CreateHashEntry(&htable, (char *)attr, &newPtr);

    if (!newPtr)
        return;
    int *hashPtr=new int[1];
    hashPtr[0]=1;
    Tcl_SetHashValue(entryPtr, hashPtr);
}

void UWDelayTimer:: handle(Event* e){
    a_->timeout((Packet*) e);
}

static class VectorbasedforwardClass : public TclClass {
public:
    VectorbasedforwardClass() : TclClass("Agent/Vectorbasedforward") {}
    TclObject* create(int argc, const char*const* argv) {
        return(new VectorbasedforwardAgent());
    }
} class_vectorbasedforward;

VectorbasedforwardAgent::VectorbasedforwardAgent() : Agent(PT_UWVB),pk_count(0),
  select_next_hop(1),node(NULL),tracetarget(NULL),width(0),counter(0),priority(1.5),delaytimer(this){
    target_ = 0;
    EnableRouting = 1;
    bind("hop_by_hop_", &hop_by_hop);
    //bind("EnableRouting", &EnableRouting);
    bind("width",&width);
    bind("select_next_hop_",&select_next_hop);
    Random::seed_heuristically();
    //bind("useOverhear_", &useOverhear);
}

#ifdef USE_MODIFIED_VBF
void VectorbasedforwardAgent::recv(Packet* packet, Handler*){
    hdr_uwvb* vbh = HDR_UWVB(packet);
    hdr_cmn* cmh = HDR_CMN(packet);
    unsigned char msg_type =vbh->mess_type;
    unsigned int dtype = vbh->data_type;
    double t1=vbh->ts_;
    position * p1;
    
    p1=new position[1];
    p1[0].x=vbh->info.fx;
    p1[0].y=vbh->info.fy;
    p1[0].z=vbh->info.fz;
  
    int pkt_target_id = cmh->next_hop();    

    if ( !EnableRouting ) {
        if ( vbh->mess_type != DATA ) {
            Packet::free(packet);
            return;
        }

        if ( vbh->sender_id.addr_ == here_.addr_ ) {
            HDR_CMN(packet)->ptype() = PT_UWVB;
            MACprepare(packet);
            MACsend(packet, Random::uniform()*JITTER);
        }
        else if ( vbh->target_id.addr_ == here_.addr_ )  {
            DataForSink(packet);
        }
        return;
    }
    
    if(msg_type == V_ACK){
      nexthop_elem find_elem(vbh->nexthop_ptr->node_id);
      vbh->nexthop_ptr->copy(find_elem);
      set<nexthop_elem>::iterator it;
      if(nexthop_set.size() > 0){
	it = nexthop_set.find(find_elem);
	if(it != nexthop_set.end() && it->node_id == vbh->nexthop_ptr->node_id){
	  nexthop_set.erase(it);
	}
      }
      nexthop_set.insert(find_elem);
      printf("Recv an ACK from (%d,%d,%f)\n",here_.addr_,vbh->nexthop_ptr->node_id,vbh->nexthop_ptr->residual_energy);
      Packet::free(packet);
      return;
    }
    /*统计选项*/
    if(here_.addr_ == 0 && vbh->forward_agent_id.addr_ == 1){
	printf("node0 recv pkt:%d from:%d\n",vbh->pk_num,vbh->forward_agent_id.addr_);
    }

printf("pkt for me: I %d recv the pkt %d for pkt_target_id: %d\n",here_.addr_,vbh->pk_num,pkt_target_id);

    /*统计选项*/
/*    if(pkt_target_id >= 0 && here_.addr_ != pkt_target_id){
    }*/
    vbf_neighborhood *hashPtr= PktTable.GetHash(vbh->sender_id, vbh->pk_num);    
    // Received this packet before
    if (hashPtr != NULL) {
	printf ("discard:vectorbasedforward:(id :%d) forward:(%d ,%d) sender is(%d,%d,%d),my position is  (%f,%f,%f)  at time %f\n",
			here_.addr_, vbh->forward_agent_id.addr_, vbh->forward_agent_id.port_,vbh->sender_id.addr_,vbh->sender_id.port_,
			vbh->pk_num,node->X(),node->Y(),node->Z(),NOW);
	/*统计选项*/
	if((pkt_target_id < 0 || here_.addr_ == pkt_target_id )&& here_.addr_ == 0){
	  printf("node0 discard pkt:%d from node:%d\n",vbh->pk_num,vbh->forward_agent_id.addr_);
	}

	PktTable.put_in_hash(vbh,p1);
	Packet::free(packet);
	printf("vectrobasedforward: node %d, pkt_num %d is duplicate packet\n",here_.addr_,vbh->pk_num);
    }else {
	// Never receive it before ? Put in hash table.
	printf("vectrobasedforward: pkt%d is new packet,\n",vbh->pk_num);
	PktTable.put_in_hash(vbh,p1);
	ConsiderNew(packet);
    }
    delete p1;
}

/*修改的VBF协议*/
void VectorbasedforwardAgent::ConsiderNew(Packet *pkt){
    hdr_cmn*  cmh = HDR_CMN(pkt);
    hdr_uwvb* vbh = HDR_UWVB(pkt);
    unsigned char msg_type =vbh->mess_type;
    unsigned int dtype = vbh->data_type;
    double l,h;
    vbf_neighborhood * hashPtr;
    ns_addr_t   from_nodeID, forward_nodeID, target_nodeID;

    Packet *gen_pkt;
    hdr_uwvb *gen_vbh;
    position * p1;
    p1=new position[1];
    p1[0].x=vbh->info.fx;
    p1[0].y=vbh->info.fy;
    p1[0].z=vbh->info.fz;
    int pkt_target_id;

    printf ("vectorbasedforward:(id :%d) forward:(%d ,%d) sender is(%d,%d,%d),my position is  (%f,%f,%f)  at time %f\n",
			    here_.addr_, vbh->forward_agent_id.addr_, vbh->forward_agent_id.port_,
			    vbh->sender_id.addr_,vbh->sender_id.port_,vbh->pk_num,
			    node->X(),node->Y(),node->Z(),NOW);
    switch (msg_type) {
    case TARGET_DISCOVERY:
        // from other nodes hitted by the packet, it is supposed
        // to be the one hop away from the sink
        if (THIS_NODE.addr_==vbh->target_id.addr_) {
            calculatePosition(pkt);
            DataForSink(pkt);
        } else  {
            Packet::free(pkt);
        }
        return;
    case SOURCE_DISCOVERY:
        Packet::free(pkt);
        return;
    case DATA_READY :
        from_nodeID = vbh->sender_id;
        if (THIS_NODE.addr_ == from_nodeID.addr_) {
            MACprepare(pkt);
            MACsend(pkt,Random::uniform()*JITTER);
            return;
        }
        calculatePosition(pkt);
        if (THIS_NODE.addr_==vbh->target_id.addr_){
            DataForSink(pkt);
        }else {
            MACprepare(pkt);
            MACsend(pkt, Random::uniform()*JITTER);
        }
        return;
    case DATA :
	from_nodeID = vbh->sender_id;
	pkt_target_id = cmh->next_hop();
	printf("pkt recved sink %d %d\n",here_.addr_,vbh->target_id.addr_);
	// come from the same node, broadcast it
        if (here_.addr_ == from_nodeID.addr_) {
	  printf("I send pkt\n");
          MACprepare(pkt);
          MACsend(pkt,0);
        }else if(here_.addr_==vbh->target_id.addr_){
	  
	  DataForSink(pkt);
	}else if(pkt_target_id<0 || 
	  (pkt_target_id>=0 && pkt_target_id == here_.addr_)){
	  calculatePosition(pkt);
	  l=advance(pkt);
	  h=projection(pkt);
	  if (IsCloseEnough(pkt)) {
	      double delay=calculateDelay(pkt,p1);
	      double d2=(UnderwaterChannel::Transmit_distance()-distance(pkt))/SPEED_OF_SOUND_IN_WATER;
	      printf("the packet target next hop id:(%d,%d,%f,%f)\n",vbh->forward_agent_id.addr_, pkt_target_id,(sqrt(delay)*DELAY+d2*2),distance(pkt));
	      set_delaytimer(pkt,(sqrt(delay)*DELAY+d2*2));
	  }else {
	      printf("not close enough: I %d discard the pkt %d\n",here_.addr_,vbh->pk_num);
	      Packet::free(pkt);
	  }
	}else{
	   printf("pkt not for me: I %d discard the pkt %d\n",here_.addr_,vbh->pk_num);
	   Packet::free(pkt);
	}
        return;

    default :
        Packet::free(pkt);
        break;
    }
    delete p1;
}

void VectorbasedforwardAgent::MACprepare(Packet *pkt){

    hdr_uwvb* vbh = HDR_UWVB(pkt);
    hdr_cmn* cmh = HDR_CMN(pkt);
    vbh->forward_agent_id = here_;
    cmh->xmit_failure_ = 0;
    /*shaoyang: select_next_hop == 1 &&  */
    if(nexthop_set.size() > 0){
      /*验证性代码：打印所有下一跳节点的代码*/
      print_all_nexthop();
      //选择第一个node作为下一跳
      int select_node_id = -1;
      double alpha = 0.5,beta = 0.5;
      double max_factor = 0.0;
      set<nexthop_elem>::iterator it;
      for(it = nexthop_set.begin();it != nexthop_set.end();++it)
      {
	double chanl_status = it->channel_status_factor;  
	//如果该后继节点的信号状况比较差，且为最近统计结果，则跳过该节点
	if(chanl_status > EPSINON){
	  
	  //如果该节点被忽略的时间已经比较长了，那么修改正它，以便下一次能用
	  if((it->omit_time) > EPSINON && (NOW - it->omit_time ) >= NEXTHOP_OMIT_TIMEOUT)
	  {
	    printf("next_hop statistics:here_:%d,被重新启用的下一跳:%d,time:%f\n",here_.addr_,it->node_id,NOW);
	    const_cast<nexthop_elem&>(*it).omit_time = 0.0;
	    const_cast<nexthop_elem&>(*it).channel_status_factor = 0.0;
	  }
	  else
	  {
	    const_cast<nexthop_elem&>(*it).omit_time = NOW;
	    printf("next_hop statistics:here_:%d,被筛掉的下一跳:%d,time:%f\n",here_.addr_,it->node_id,NOW);	    
	    continue;
	  }
	}
	else{
	  //为每个就节点计算渴望因子
	  double desire_factor = evaluate_factor(pkt,it->pos);
	  double res_energy = it->residual_energy;
	  //double factor = alpha*res_energy + beta/desire_factor;
	  assert(desire_factor > 0.00001);
	  double factor = 1/desire_factor;
//  	  if(here_.addr_ == 5)
//  	      printf("next_hop statistics:here_:%d, nexthop_node:%d,desire_factor:%f,energy:%f,channel_status_factor:%f\n",
//  		  here_.addr_,it->node_id,desire_factor,res_energy,chanl_status);

	  if(factor > max_factor){
	    max_factor = factor;
	    select_node_id = it->node_id;
	  }
	}
      }
      cmh->next_hop() = select_node_id;
    }else{
      cmh->next_hop() = MAC_BROADCAST;
    }
    
    printf("Vectorbasedforward evaluate factor:%d %d\n",here_.addr_,cmh->next_hop());

    cmh->addr_type() = NS_AF_ILINK;
    cmh->direction() = hdr_cmn::DOWN;
    if (!node->sinkStatus()) {
        vbh->info.fx=node->CX();
        vbh->info.fy=node->CY();
        vbh->info.fz=node->CZ();
    }
    else {
        vbh->info.fx=node->X();
        vbh->info.fy=node->Y();
        vbh->info.fz=node->Z();
    }
}

/*在上一跳节点已经选择好下一跳由谁转发，因此在这里不用进行太多计算*/
void VectorbasedforwardAgent::timeout(Packet * pkt) {
    hdr_cmn* cmh = hdr_cmn::access(pkt);
    hdr_uwvb* vbh = HDR_UWVB(pkt);
    unsigned char msg_type =vbh->mess_type;
    vbf_neighborhood  *hashPtr;

    switch (msg_type) {
    case DATA:
	MACprepare(pkt);
	MACsend(pkt,0);
        break;
    default:
        Packet::free(pkt);
        break;
    }
}
#endif

#ifndef USE_MODIFIED_VBF

void VectorbasedforwardAgent::recv(Packet* packet, Handler*)
{
    hdr_uwvb* vbh = HDR_UWVB(packet);
    unsigned char msg_type =vbh->mess_type;
    unsigned int dtype = vbh->data_type;
    double t1=vbh->ts_;
    position * p1;

    p1=new position[1];
    p1[0].x=vbh->info.fx;
    p1[0].y=vbh->info.fy;
    p1[0].z=vbh->info.fz;
    if ( !EnableRouting ) {
        if ( vbh->mess_type != DATA ) {
            Packet::free(packet);
            return;
        }

        if ( vbh->sender_id.addr_ == here_.addr_ ) {
            HDR_CMN(packet)->ptype() = PT_UWVB;
            MACprepare(packet);
            MACsend(packet, Random::uniform()*JITTER);
        }
        else if ( vbh->target_id.addr_ == here_.addr_ )  {
            DataForSink(packet);
        }
        return;
    }
    vbf_neighborhood *hashPtr= PktTable.GetHash(vbh->sender_id, vbh->pk_num);
    // Received this packet before 
    if (hashPtr != NULL) {
	hdr_uwvb* vbh = HDR_UWVB(packet);
	printf ("discard:vectorbasedforward:(id :%d) forward:(%d ,%d) sender is(%d,%d,%d),my position is  (%f,%f,%f)  at time %f\n",
			here_.addr_, vbh->forward_agent_id.addr_, vbh->forward_agent_id.port_,vbh->sender_id.addr_,vbh->sender_id.port_,
			vbh->pk_num,node->X(),node->Y(),node->Z(),NOW);
	PktTable.put_in_hash(vbh,p1);
	drop_->recv(packet,"Error/Collision");
	Packet::free(packet);
        //printf("vectrobasedforward: this is duplicate packet\n");
    }else {
	hdr_cmn* cmh = HDR_CMN(packet);
	set<neighbor_node_elem> neighbor_nodes =  node->neighbor_nodes.neighbor_node_set;
	set<neighbor_node_elem>::iterator it = neighbor_nodes.begin();
	neighbor_node_elem em = *it;
	cmh->next_hop_ = em.node_id;
	printf("node id:%d next_hop:%d\n",node->nodeid(),cmh->next_hop());
	hdr_cmn* cmh2 = HDR_CMN(packet);
        
        printf("vectrobasedforward: this is new packet,\n");
	PktTable.put_in_hash(vbh,p1);
        ConsiderNew(packet);
    }

    delete p1;
}

/*原始ConsiderNew函数*/
void VectorbasedforwardAgent::ConsiderNew(Packet *pkt){
    hdr_uwvb* vbh = HDR_UWVB(pkt);
    unsigned char msg_type =vbh->mess_type;
    unsigned int dtype = vbh->data_type;
    double l,h;
    vbf_neighborhood * hashPtr;
    ns_addr_t   from_nodeID, forward_nodeID, target_nodeID;

    Packet *gen_pkt;
    hdr_uwvb *gen_vbh;
    position * p1;
    p1=new position[1];
    p1[0].x=vbh->info.fx;
    p1[0].y=vbh->info.fy;
    p1[0].z=vbh->info.fz;

    printf ("vectorbasedforward:(id :%d) forward:(%d ,%d) sender is(%d,%d,%d),my position is  (%f,%f,%f)  at time %f\n",
			    here_.addr_, vbh->forward_agent_id.addr_, vbh->forward_agent_id.port_,
			    vbh->sender_id.addr_,vbh->sender_id.port_,vbh->pk_num,
			    node->X(),node->Y(),node->Z(),NOW);
    switch (msg_type) {
      case INTEREST :
        printf("Vectorbasedforward:it is interest packet!\n");
        hashPtr = PktTable.GetHash(vbh->sender_id, vbh->pk_num);
        from_nodeID = vbh->sender_id;
        forward_nodeID = vbh->forward_agent_id;
        if (THIS_NODE.addr_ == from_nodeID.addr_) {

            MACprepare(pkt);
            MACsend(pkt,Random::uniform()*JITTER);
            //  printf("vectorbasedforward: after MACprepare(pkt)\n");
        }
        else
        {
            calculatePosition(pkt);
            //printf("vectorbasedforward: This packet is from different node\n");
            if (IsTarget(pkt))
            {
                // If this node is target?
                l=advance(pkt);

                //  send_to_demux(pkt,0);
                //  printf("vectorbasedforward:%d send out the source-discovery \n",here_.addr_);
                vbh->mess_type=SOURCE_DISCOVERY;
                set_delaytimer(pkt,l*JITTER);
                // !!! need to re-think
            }
            else {
                // calculatePosition(pkt);
                // No the target forwared
                l=advance(pkt);
                h=projection(pkt);
                if (IsCloseEnough(pkt)) {
                    // printf("vectorbasedforward:%d I am close enough for the interest\n",here_.addr_);
                    MACprepare(pkt);
                    MACsend(pkt,Random::uniform()*JITTER);//!!!! need to re-think
                }
                else {
                    //  printf("vectorbasedforward:%d I am not close enough for the interest  \n",here_.addr_);
                    Packet::free(pkt);
                }
            }
        }
        // Packet::free(pkt);
        return;
    case TARGET_DISCOVERY:
        // from other nodes hitted by the packet, it is supposed
        // to be the one hop away from the sink
        // printf("Vectorbasedforward(%d,%d):it is target-discovery  packet(%d)! it target id is %d  coordinate is %f,%f,%f and range is %f\n",here_.addr_,here_.port_,vbh->pk_num,vbh->target_id.addr_,vbh->info.tx, vbh->info.ty,vbh->info.tz,vbh->range);
        if (THIS_NODE.addr_==vbh->target_id.addr_) {
            //printf("Vectorbasedforward(%d,%d):it is target-discovery  packet(%d)! it target id is %d  coordinate is %f,%f,%f and range is %f\n",here_.addr_,here_.port_,vbh->pk_num,vbh->target_id.addr_,vbh->info.tx, vbh->info.ty,vbh->info.tz,vbh->range);
            // ns_addr_t *hashPtr= PktTable.GetHash(vbh->sender_id, vbh->pk_num);
            // Received this packet before ?
            // if (hashPtr == NULL) {

            calculatePosition(pkt);
            DataForSink(pkt);
            printf("Vectorbasedforward: %d is the target\n", here_.addr_);
            // } //New data Process this data
            //
        } else  {
            Packet::free(pkt);
        }
        return;

    case SOURCE_DISCOVERY:
        Packet::free(pkt);
        // other nodes already claim to be the source of this interest
        //   SourceTable.put_in_hash(vbh);
        return;

    case DATA_READY :
        //  printf("Vectorbasedforward(%d,%d):it is data ready packet(%d)! it target id is %d \n",here_.addr_,here_.port_,vbh->pk_num,vbh->target_id.addr_);
        from_nodeID = vbh->sender_id;
        if (THIS_NODE.addr_ == from_nodeID.addr_) {
            // come from the same node, broadcast it
            MACprepare(pkt);
            MACsend(pkt,Random::uniform()*JITTER);
            return;
        }
        calculatePosition(pkt);
        if (THIS_NODE.addr_==vbh->target_id.addr_)
        {
            printf("Vectorbasedforward: %d is the target\n", here_.addr_);
            DataForSink(pkt); // process it
        }
        else {
            // printf("Vectorbasedforward: %d is the not  target\n", here_.addr_);
            MACprepare(pkt);
            MACsend(pkt, Random::uniform()*JITTER);
        }
        return;

    case DATA :
        // printf("Vectorbasedforward(%d,%d):it is data packet(%d)! it target id is %d  coordinate is %f,%f,%f and range is %f\n",here_.addr_,here_.port_,vbh->pk_num,vbh->target_id.addr_,vbh->info.tx, vbh->info.ty,vbh->info.tz,vbh->range);
        printf("Vectorbasedforward(%d):it is data packet(%d)\n",here_.addr_,vbh->pk_num);
        from_nodeID = vbh->sender_id;
        // come from the same node, broadcast it
        if (THIS_NODE.addr_ == from_nodeID.addr_) {
            MACprepare(pkt);
            MACsend(pkt,0);
            return;
        }
        calculatePosition(pkt);
        l=advance(pkt);
        h=projection(pkt);

        if (THIS_NODE.addr_==vbh->target_id.addr_)
        {
            DataForSink(pkt); // process it
        }
        else {
            if (IsCloseEnough(pkt)) {
                double delay=calculateDelay(pkt,p1);
                double d2=(UnderwaterChannel::Transmit_distance()-distance(pkt))/SPEED_OF_SOUND_IN_WATER;
                printf("Vectorbasedforward: I am  not  target delay is %f d2=%f distance=%f \n",(sqrt(delay)*DELAY+d2*2),d2,distance(pkt));
                set_delaytimer(pkt,(sqrt(delay)*DELAY+d2*2));
            }
            else {
                Packet::free(pkt);
            }
        }
        return;

    default :

        Packet::free(pkt);
        break;
    }
    delete p1;
}

void VectorbasedforwardAgent::MACprepare(Packet *pkt){
    hdr_uwvb* vbh = HDR_UWVB(pkt);
    hdr_cmn* cmh = HDR_CMN(pkt);
    vbh->forward_agent_id = here_;
    cmh->xmit_failure_ = 0;
    cmh->addr_type() = NS_AF_ILINK;
    cmh->direction() = hdr_cmn::DOWN;
    if (!node->sinkStatus()) {     //!! I add new part
        vbh->info.fx=node->CX();
        vbh->info.fy=node->CY();
        vbh->info.fz=node->CZ();
    }
    else {
        vbh->info.fx=node->X();
        vbh->info.fy=node->Y();
        vbh->info.fz=node->Z();
    }
}

void VectorbasedforwardAgent::timeout(Packet * pkt) {
    hdr_uwvb* vbh = HDR_UWVB(pkt);
    unsigned char msg_type =vbh->mess_type;
    vbf_neighborhood  *hashPtr;

    switch (msg_type) {
    case DATA:
        hashPtr= PktTable.GetHash(vbh->sender_id, vbh->pk_num);
        if (hashPtr != NULL) {
            int num_neighbor=hashPtr->number;
            //printf("vectorbasedforward: node %d have received %d when wake up at %f\n",here_.addr_,num_neighbor,NOW);
	    int it=hdr_cmn::access(pkt)->next_hop();
	    printf("the packet is for node:%d \n",it);
	    if (num_neighbor!=1) {
                // Some guys transmit the data before me
                if (num_neighbor==MAX_NEIGHBOR) {
                    //I have too many neighbors, I quit
                    Packet::free(pkt);
                    return;
                }
                else {
		//I need to calculate my delay time again
                    int i=0;
                    position* tp;
                    tp=new position[1];

                    tp[0].x=hashPtr->neighbor[i].x;
                    tp[0].y=hashPtr->neighbor[i].y;
                    tp[0].z=hashPtr->neighbor[i].z;
                    double tdelay=calculateDelay(pkt,tp);
                    // double tdelay=5;
                    i++;
                    double c=1;
                    while (i<num_neighbor) {
                        c=c*2;
                        tp[0].x=hashPtr->neighbor[i].x;
                        tp[0].y=hashPtr->neighbor[i].y;
                        tp[0].z=hashPtr->neighbor[i].z;
                        double t2delay=calculateDelay(pkt,tp);
                        if (t2delay<tdelay)
                            tdelay=t2delay;
                        i++;
                    }

                    delete tp;
                    if (tdelay<=(priority/c)) {
			MACprepare(pkt);
			MACsend(pkt,0);
                    }
                    else
                        Packet::free(pkt);//to much overlap, don;t send
                }// end of calculate my new delay time
            }
            else {// I am the only neighbor
                position* tp;
                tp=new position[1];
                tp[0].x=vbh->info.fx;
                tp[0].y=vbh->info.fy;
                tp[0].z=vbh->info.fz;
                double delay=calculateDelay(pkt,tp);

                delete tp;
                if (delay<=priority) {
		    MACprepare(pkt);
		    MACsend(pkt,0);
                }
                else  
		  Packet::free(pkt);
                return;
            }
        }
        break;
    default:
        Packet::free(pkt);
        break;
    }
}
#endif

void VectorbasedforwardAgent::reset(){
    PktTable.reset();
}

void VectorbasedforwardAgent::Terminate(){
#ifdef DEBUG_OUTPUT
    printf("node %d: remaining energy %f, initial energy %f\n", THIS_NODE,
           node->energy_model()->energy(),
           node->energy_model()->initialenergy() );
#endif
}

void VectorbasedforwardAgent::StopSource(){
    /*
    Agent_List *cur;

    for (int i=0; i<MAX_DATA_TYPE; i++) {
    for (cur=routing_table[i].source; cur!=NULL; cur=AGENT_NEXT(cur) ) {
    SEND_MESSAGE(i, AGT_ADDR(cur), DATA_STOP);
    }
    }
    */
}

Packet * VectorbasedforwardAgent:: create_packet(){
    Packet *pkt = allocpkt();
    if (pkt==NULL) 
      return NULL;

    hdr_cmn*  cmh = HDR_CMN(pkt);
    cmh->size() = 36;
    hdr_uwvb* vbh = HDR_UWVB(pkt);
    vbh->ts_ = NOW;
    //!! I add new part
    vbh->info.ox=node->CX();
    vbh->info.oy=node->CY();
    vbh->info.oz=node->CZ();
    vbh->info.fx=node->CX();
    vbh->info.fy=node->CY();
    vbh->info.fz=node->CZ();
    return pkt;
}

Packet *VectorbasedforwardAgent::prepare_message(unsigned int dtype, ns_addr_t to_addr,  int msg_type){
    Packet *pkt;
    hdr_uwvb *vbh;
    //hdr_ip *iph;

    pkt = create_packet();
    vbh = HDR_UWVB(pkt);
    // iph = HDR_IP(pkt);

    vbh->mess_type = msg_type;
    vbh->pk_num = pk_count;
    pk_count++;
    vbh->sender_id = here_;
    vbh->data_type = dtype;
    vbh->forward_agent_id = here_;

    vbh->ts_ = NOW;
    return pkt;
}

void VectorbasedforwardAgent::print_all_nexthop(){
  set<nexthop_elem>::iterator it;
  printf("node:%d\n",here_.addr_);
  for(it = nexthop_set.begin();it != nexthop_set.end();++it){
    printf("next_hop_node_id:%d\n",it->node_id);
  }
}

/*shaoyang added*/
double VectorbasedforwardAgent::evaluate_factor(Packet* pkt,position pos){
  double factor = 0.0,delay,d2;
  position *p1 = &pos;
  delay=calculateDelay(pkt,p1);
  printf("float exception:%f %d %f %f %f\n",delay,SPEED_OF_SOUND_IN_WATER,p1->x,p1->y,p1->z);
  d2=(UnderwaterChannel::Transmit_distance()-distance(pkt))/SPEED_OF_SOUND_IN_WATER;
  factor = sqrt(delay) * DELAY + d2*2;
  return factor;
}

void VectorbasedforwardAgent::MACsend(Packet *pkt, Time delay){
   Scheduler::instance().schedule(ll, pkt, delay);
}

void VectorbasedforwardAgent::DataForSink(Packet *pkt){
    send_to_dmux(pkt, 0);
}

void VectorbasedforwardAgent::trace (char *fmt,...){
    va_list ap;
    if (!tracetarget)
        return;
    va_start (ap, fmt);
    vsprintf (tracetarget->pt_->buffer(), fmt, ap);
    tracetarget->pt_->dump ();
    va_end (ap);
}

void VectorbasedforwardAgent::set_delaytimer(Packet* pkt, double c) {
    Scheduler& s=Scheduler::instance();
    s.schedule(&delaytimer,(Event*)pkt,c);
    return;
}

void VectorbasedforwardAgent::calculatePosition(Packet* pkt){
    hdr_uwvb     *vbh  = HDR_UWVB(pkt);
    double fx=vbh->info.fx;
    double fy=vbh->info.fy;
    double fz=vbh->info.fz;

    double dx=vbh->info.dx;
    double dy=vbh->info.dy;
    double dz=vbh->info.dz;

    node->CX_=fx+dx;
    node->CY_=fy+dy;
    node->CZ_=fz+dz;
    // printf("vectorbased: my position is computed as (%f,%f,%f)\n",node->CX_, node->CY_,node->CZ_);
}

double VectorbasedforwardAgent::calculateDelay(Packet* pkt,position* p1){
    hdr_uwvb *vbh  = HDR_UWVB(pkt);
    
    double fx=p1->x;
    double fy=p1->y;
    double fz=p1->z;

    double dx=node->CX_-fx;
    double dy=node->CY_-fy;
    double dz=node->CZ_-fz;

    double tx=vbh->info.tx;
    double ty=vbh->info.ty;
    double tz=vbh->info.tz;

    double dtx=tx-fx;
    double dty=ty-fy;
    double dtz=tz-fz;

    double dp=dx*dtx+dy*dty+dz*dtz;
    double p=projection(pkt);
    double d=sqrt((dx*dx)+(dy*dy)+(dz*dz));
    double l=sqrt((dtx*dtx)+(dty*dty)+ (dtz*dtz));
    
    double cos_theta=dp/(d*l);
    double delay=(p/width) +((UnderwaterChannel::Transmit_distance()-d*cos_theta)/UnderwaterChannel::Transmit_distance());
    return delay;
}

double VectorbasedforwardAgent::distance(Packet* pkt){
    hdr_uwvb     *vbh  = HDR_UWVB(pkt);
    double tx=vbh->info.fx;
    double ty=vbh->info.fy;
    double tz=vbh->info.fz;
    // printf("vectorbased: the target is %lf,%lf,%lf \n",tx,ty,tz);
    double x=node->CX(); //change later
    double y=node->CY();// printf(" Vectorbasedforward: I am in advanced\n");
    double z=node->CZ();
    // printf("the target is %lf,%lf,%lf and my coordinates are %lf,%lf,%lf\n",tx,ty,tz,x,y,z);
    return sqrt((tx-x)*(tx-x)+(ty-y)*(ty-y)+ (tz-z)*(tz-z));
}

double VectorbasedforwardAgent::advance(Packet* pkt){
    hdr_uwvb     *vbh  = HDR_UWVB(pkt);
    double tx=vbh->info.tx;
    double ty=vbh->info.ty;
    double tz=vbh->info.tz;
    // printf("vectorbased: the target is %lf,%lf,%lf \n",tx,ty,tz);
    double x=node->CX(); //change later
    double y=node->CY();
    double z=node->CZ();
    return sqrt((tx-x)*(tx-x)+(ty-y)*(ty-y)+ (tz-z)*(tz-z));
}

double VectorbasedforwardAgent::projection(Packet* pkt){
    hdr_uwvb *vbh  = HDR_UWVB(pkt);

    double tx=vbh->info.tx;
    double ty=vbh->info.ty;
    double tz=vbh->info.tz;

    double ox, oy, oz;

    if ( !hop_by_hop )
    {
        //printf("vbf is used\n");
        ox=vbh->info.ox;
        oy=vbh->info.oy;
        oz=vbh->info.oz;
    }
    else {
        //printf("hop_by_hop vbf is used\n");
        ox=vbh->info.fx;
        oy=vbh->info.fy;
        oz=vbh->info.fz;
    }

    double x=node->CX();
    double y=node->CY();
    double z=node->CZ();

    double wx=tx-ox;
    double wy=ty-oy;
    double wz=tz-oz;

    double vx=x-ox;
    double vy=y-oy;
    double vz=z-oz;

    double cross_product_x=vy*wz-vz*wy;
    double cross_product_y=vz*wx-vx*wz;
    double cross_product_z=vx*wy-vy*wx;

    double area=sqrt(cross_product_x*cross_product_x+
                     cross_product_y*cross_product_y+cross_product_z*cross_product_z);
    double length=sqrt((tx-ox)*(tx-ox)+(ty-oy)*(ty-oy)+ (tz-oz)*(tz-oz));
    
    return area/length;
}

bool VectorbasedforwardAgent::IsTarget(Packet* pkt){
    hdr_uwvb * vbh=HDR_UWVB(pkt);

    if (vbh->target_id.addr_==0) {
        return (advance(pkt)<vbh->range);
    }
    else 
		return(THIS_NODE.addr_==vbh->target_id.addr_);
}

bool VectorbasedforwardAgent::IsCloseEnough(Packet* pkt){
    hdr_uwvb *vbh  = HDR_UWVB(pkt);
    double range=vbh->range;
    //double range=width;
    double ox=vbh->info.ox;
    double oy=vbh->info.oy;
    double oz=vbh->info.oz;

    double tx=vbh->info.tx;
    double ty=vbh->info.ty;
    double tz=vbh->info.tz;

    double fx=vbh->info.fx;
    double fy=vbh->info.fy;
    double fz=vbh->info.fz;

    double x=node->CX(); 		//change later
    double y=node->CY();
    double z=node->CZ();

    double d=sqrt((tx-fx)*(tx-fx)+(ty-fy)*(ty-fy)+(tz-fz)*(tz-fz));
    //double l=sqrt((tx-ox)*(tx-ox)+(ty-oy)*(ty-oy)+ (tz-oz)*(tz-oz));
    double l=advance(pkt);
    double c=1;
    double prj = projection(pkt);
    if ((prj <= (c*width)))  return true;
    return false;

}

int VectorbasedforwardAgent::command(int argc, const char*const* argv){
    Tcl& tcl =  Tcl::instance();

    if (argc == 2) {

        if (strcasecmp(argv[1], "reset-state")==0) {

            reset();
            return TCL_OK;
        }

        if (strcasecmp(argv[1], "reset")==0) {

            return Agent::command(argc, argv);
        }

        if (strcasecmp(argv[1], "start")==0) {
            return TCL_OK;
        }

        if (strcasecmp(argv[1], "stop")==0) {
            return TCL_OK;
        }

        if (strcasecmp(argv[1], "terminate")==0) {
            Terminate();
            return TCL_OK;
        }

        if (strcasecmp(argv[1], "name")==0) {
            //printf("vectorbased \n");
            return TCL_OK;
        }
        if (strcasecmp(argv[1], "stop-source")==0) {
            StopSource();
            return TCL_OK;
        }

    } else if (argc == 3) {

        if (strcasecmp(argv[1], "on-node")==0) {
            //   printf ("inside on node\n");
            node = (UnderwaterSensorNode *)tcl.lookup(argv[2]);
            return TCL_OK;
        }
        /*
        if (strcasecmp(argv[1], "set-port")==0) {
        printf ("inside on node\n");
        port_number=atoi(argv[2]);
        return TCL_OK;
        }
        */
        if (strcasecmp(argv[1], "add-ll") == 0) {

            TclObject *obj;
			printf("%s\n",argv[2]);
            if ( (obj = TclObject::lookup(argv[2])) == 0) {
                fprintf(stderr, "Vectorbasedforwarding Node: %d lookup of %s failed\n", THIS_NODE.addr_, argv[2]);
                return TCL_ERROR;
            }
            ll = (NsObject *) obj;

            return TCL_OK;
        }

        if (strcasecmp (argv[1], "tracetarget") == 0) {
            TclObject *obj;
            if ((obj = TclObject::lookup (argv[2])) == 0) {
                fprintf (stderr, "%s: %s lookup of %s failed\n", __FILE__, argv[1],
                         argv[2]);
                return TCL_ERROR;
            }

            tracetarget = (Trace *) obj;
            return TCL_OK;
        }

        if (strcasecmp(argv[1], "port-dmux") == 0) {
            // printf("vectorbasedforward:port demux is called \n");
            TclObject *obj;

            if ( (obj = TclObject::lookup(argv[2])) == 0) {
                fprintf(stderr, "VB node Node: %d lookup of %s failed\n", THIS_NODE.addr_, argv[2]);
                return TCL_ERROR;
            }
            port_dmux = (NsObject *) obj;
            return TCL_OK;
        }

    }

    return Agent::command(argc, argv);
}

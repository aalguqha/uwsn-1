#include "packet.h"
#include "random.h"
#include "mac.h"
#include "uwaloha.h"
#include "../underwaterphy.h"
#include "underwatersensor/uw_common/underwatersensornode.h"

#define CHANNEL_STATUS_EVALUATING
using namespace std;

int hdr_UWALOHA::offset_;
static class UWALOHA_HeaderClass: public PacketHeaderClass{
public:
	UWALOHA_HeaderClass():PacketHeaderClass("PacketHeader/UWALOHA",sizeof(hdr_UWALOHA))
	{
		bind_offset(&hdr_UWALOHA::offset_);
	}
}class_UWALOHA_hdr;

static class UWALOHAClass : public TclClass {
public:
	UWALOHAClass():TclClass("Mac/UnderwaterMac/UWALOHA") {}
	TclObject* create(int, const char*const*) {
		return (new UWALOHA());
	}
}class_UWALOHA;

/*===========================UWALOHA Timer===========================*/
long UWALOHA_ACK_RetryTimer::id_generator = 0;

void UWALOHA_ACK_RetryTimer::expire(Event* e){
	  mac_->processRetryTimer(this);
}

/*超时则重新发送数据包*/
void UWALOHA_BackoffTimer::expire(Event *e){
	UnderwaterSensorNode *tnode = (UnderwaterSensorNode *)mac_->node();
	printf("Timeout Backoff:I re-send packet node:%d at %f\n",tnode->nodeid(),NOW);
	/*这里是有一定概率回退的，并不是完全重发*/
	mac_->sendDataPkt();
}

/*超时回退，并不立即发送数据包*/
void UWALOHA_WaitACKTimer::expire(Event *e){
  	UnderwaterSensorNode *tnode = (UnderwaterSensorNode *)mac_->node();
	printf("Time out: waitACK node:%d at %f\n",tnode->nodeid(),NOW);
	mac_->doBackoff();
}

//construct function
UWALOHA::UWALOHA(): UnderwaterMac(), bo_counter(0), UWALOHA_Status(PASSIVE), Persistent(1.0),
		ACKOn(1), Min_Backoff(0.0), Max_Backoff(1.5), MAXACKRetryInterval(0.05),WAIT_ACK_NODE(-1),
		blocked(false), chan_status_table(new Chanl_Status_Table(MAX_CHANL_STATUS_SIZE)),
		BackoffTimer(this), WaitACKTimer(this),
		CallBack_handler(this), status_handler(this)
{
	MaxPropDelay = UnderwaterChannel::Transmit_distance()/1500.0;
	bind("Persistent",&Persistent);
	bind("ACKOn",&ACKOn);
	bind("Min_Backoff",&Min_Backoff);
	bind("Max_Backoff",&Max_Backoff);
	bind("WaitACKTime",&WaitACKTime);
	bind("ACKSizeRatio",&AckSizeRatio);
}

void UWALOHA::doBackoff(){
	  printf("waiting ACK node doBackoff %d,at %f \n", index_,NOW);
	  Time BackoffTime=Random::uniform(Min_Backoff,Max_Backoff);
	  bo_counter++;
	  if (bo_counter < MAXIMUMCOUNTER) {
		  UWALOHA_Status = BACKOFF;
		  //重新backoff BackoffTime的时间
		  BackoffTimer.resched(BackoffTime);
	  }//如果bo_counter达到最大值，那么就需要重新发送了
	  else {
		  bo_counter=0;
		  printf("backoffhandler: too many backoffs,decide re-transmission\n");
		  Packet::free(PktQ_.front());
		  PktQ_.pop();
		  /*start shaoyang*/
		  UWALOHA_Status = PASSIVE;
		  blocked =  false;
		  /*end shaoyang*/
		  processPassive();
	  }
}

void UWALOHA::processPassive(){
  if (UWALOHA_Status == PASSIVE && !blocked) {
    if (!PktQ_.empty())
	    sendDataPkt();
  }
}

void UWALOHA_StatusHandler::handle(Event *e){
  mac_->StatusProcess(is_ack_);
}

void UWALOHA_CallBackHandler::handle(Event* e){
  mac_->CallbackProcess(e);
}

void UWALOHA::CallbackProcess(Event* e){
  callback_->handle(e);
}

void UWALOHA::StatusProcess(bool is_ack){
	UnderwaterSensorNode* n=(UnderwaterSensorNode*) node_;
	n->SetTransmissionStatus(IDLE);
	
	if( blocked ) {
		blocked = false;
		processPassive();
		return;
	}
	
	if( !ACKOn ) {
		/*Must be DATA*/
		UWALOHA_Status = PASSIVE;
		processPassive();
	}
	else if (ACKOn && !is_ack ) {
		UWALOHA_Status = WAIT_ACK;
	}
}
/*===========================Send and Receive===========================*/

void UWALOHA::TxProcess(Packet* pkt){
	Scheduler::instance().schedule(&CallBack_handler, &callback_event, CALLBACK_DELAY);

	hdr_cmn* cmh = HDR_CMN(pkt);
	hdr_UWALOHA* UWALOHAh = hdr_UWALOHA::access(pkt);
	cmh->size() += hdr_UWALOHA::size();
	cmh->txtime() = getTxTime(cmh->size());
	cmh->error() = 0;
	cmh->direction() = hdr_cmn::DOWN;

	Time t = NOW;
	if( t > 500 ) 
	  t = NOW;
	UWALOHAh->packet_type = hdr_UWALOHA::UW_DATA;
	UWALOHAh->SA = index_;
	//(nsaddr_t)IP_BROADCAST
	if( cmh->next_hop() == MAC_BROADCAST ) {
		UWALOHAh->DA = MAC_BROADCAST;
	}
	else {
		UWALOHAh->DA = cmh->next_hop();
	}

	PktQ_.push(pkt);//push packet to the queue
	
	//fill the next hop when sending out the packet;
	if(UWALOHA_Status == PASSIVE 
		&& PktQ_.size() == 1 && !blocked ){
		printf("I am sending pkt:\n");
		sendDataPkt();
	}
}

void UWALOHA::sendDataPkt(){
	double P = Random::uniform(0,1);
	Packet* tmp = PktQ_.front();
	UWALOHA_Status = SEND_DATA;

	if( P<=Persistent ) {
	  sendPkt(tmp->copy());
	}
	else {
	  bo_counter--;
	  doBackoff();
	}
	return;
}

void UWALOHA::sendPkt(Packet *pkt){
	hdr_cmn* cmh=HDR_CMN(pkt);
	hdr_UWALOHA* UWALOHAh = hdr_UWALOHA::access(pkt);

	cmh->direction() = hdr_cmn::DOWN;
	
	UnderwaterSensorNode* n=(UnderwaterSensorNode*) node_;
	double txtime=cmh->txtime();
	Scheduler& s=Scheduler::instance();

	switch( n->TransmissionStatus() ) {
		case SLEEP:
			Poweron();
		case IDLE:
		  
			n->SetTransmissionStatus(SEND); 
			cmh->timestamp() = NOW;
			cmh->direction() = hdr_cmn::DOWN;
			
			//ACK doesn't affect the status, only process DATA here
			if (UWALOHAh->packet_type == hdr_UWALOHA::UW_DATA) {
				//must be a DATA packet, so setup wait ack timer
				//接受到的为数据包，并且不是BROADCAST方式，ACKOn是true
				if ((UWALOHAh->DA != (nsaddr_t)MAC_BROADCAST) && ACKOn) {
					UWALOHA_Status = WAIT_ACK;
					WAIT_ACK_NODE = UWALOHAh->DA;
					WaitACKTimer.resched(WaitACKTime+txtime);
					status_handler.is_ack() = false;
printf("SKGD:Send Data,node:%d,target_node:%d,pkt_num:%d waitTime:%f at %f\n",
index_,UWALOHAh->DA,hdr_uwvb::access(pkt)->pk_num,WaitACKTime+txtime,NOW);
					/*添加信道状况统计信息，判断指针是否为空*/
					hdr_uwvb * vbh = hdr_uwvb::access(pkt);
					int has_been_ack = 0;
					if(chan_status_table != NULL){
					  /*信道状况统计结构体*/
					  chanl_status status(UWALOHAh->DA,vbh->pk_num,has_been_ack);
					  status.send_time = NOW;	//其他时间均为0
					  printf("%d %d %f\n",status.target_id,status.pkt_id,status.send_time);
					  chan_status_table->put_into_table(status);
					  chan_status_table->print_all_elements();
					}
					/*添加信道信息结束*/
				}
				else {
					//这两句有啥作用，直接将包释放掉了,这里可以先不着急释放，等的哦阿在
					//vbf中再释放也不迟。
					Packet::free(PktQ_.front());
					PktQ_.pop();
					UWALOHA_Status = PASSIVE;
					//UWALOHA_Status = WAIT_ACK;
					//WaitACKTimer.resched(WaitACKTime + txtime);
				}
			}
			else{
				status_handler.is_ack() = true;
			}

			sendDown(pkt);		//UnderwaterMAC.cc
			blocked = true;
			s.schedule(&status_handler,&status_event,txtime);
			break;
			
		case RECV:
			printf("RECV-SEND Collision!!!!!\n");
			if( UWALOHAh->packet_type == hdr_UWALOHA::UW_ACK ) 
				retryACK(pkt);
			else
				Packet::free(pkt);
			
			UWALOHA_Status = PASSIVE;
			break;
			
		default:
		//status is SEND
			printf("node%d send data too fast\n",index_);
			if( UWALOHAh->packet_type == hdr_UWALOHA::UW_ACK ) 
				retryACK(pkt);
			else
				Packet::free(pkt);
			UWALOHA_Status = PASSIVE;
	}
}

void UWALOHA::RecvProcess(Packet *pkt){
	hdr_UWALOHA* UWALOHAh = hdr_UWALOHA::access(pkt);
	hdr_cmn* cmh=HDR_CMN(pkt);
	nsaddr_t recver = UWALOHAh->DA;

	if( cmh->error() ) 
	{
	  if(drop_ && recver==index_) {
		  drop_->recv(pkt,"Error/Collision");
	  }
	  else
		  Packet::free(pkt);
	  //processPassive();
	  return;
	}
	//这里是接收到反馈的ACK数据
	if( UWALOHAh->packet_type == hdr_UWALOHA::UW_ACK ) {
	    //if get ACK after WaitACKTimer, ignore ACK
	    if( recver == index_){
	      if(UWALOHA_Status == WAIT_ACK) {
printf("SKGD:WaitACK,node:%d,ACK_from_node:%d,pkt_num:%d at %f\n",
index_,UWALOHAh->SA,UWALOHAh->ack_pkt_no,NOW);
		if(passBack(pkt,0)==0)
		  sendUp(pkt->copy());

		WaitACKTimer.cancel();
		bo_counter=0;
		Packet::free(PktQ_.front());
		PktQ_.pop();
		//处理完ACK后将status的状态修改为PASSIVE
		UWALOHA_Status=PASSIVE;
		processPassive();
		/*更新chanl_status_table中信息*/
		int has_been_ack = 1;
		chanl_status status(UWALOHAh->SA,UWALOHAh->ack_pkt_no,1);
		status.ack_time = NOW;
		chan_status_table->ack_elem(status);
		chan_status_table->print_all_elements();
	      }else{
printf("SKGD:WaitACK,node:%d,ACK_from_node:%d,pkt_num:%d at %f\n",
index_,UWALOHAh->SA,hdr_UWALOHA::access(pkt)->ack_pkt_no,NOW);

		if(passBack(pkt,0)==0)
		  sendUp(pkt->copy());
	      }
	    }
	}
	else if(UWALOHAh->packet_type == hdr_UWALOHA::UW_DATA) {
	    //process Data packet
	  if(recver == index_ || recver == (nsaddr_t)MAC_BROADCAST ) {
		    hdr_uwvb* vbh = HDR_UWVB(pkt);
		    /*3号泊松丢包*/
		    if(index_== 4){
		      bool poi_factor = uw_tools.ifSend(NOW);
		      if(poi_factor == false){
			printf("I discard PKT from (%d,%d) pkt_num:%d\n",index_,vbh->forward_agent_id.addr_,vbh->pk_num);
			drop_->recv(pkt,"Error/Collision");
			Packet::free(pkt);
			return;
		      }
		    }
		    cmh->size() -= hdr_UWALOHA::size();
		    sendUp(pkt->copy());	//UnderwaterMAC.cc
		    //if ( ACKOn && (recver != (nsaddr_t)MAC_BROADCAST))
		      /*shaoyang：判断该DATA是否由前向节点回传,判断my比fwd更接近tgt*/	
		    if (passBack(pkt,0)){
		      
printf("SKGD:SendACK,node:%d,forward_node:%d,pkt_num:%d at %f\n",
index_,UWALOHAh->SA,hdr_uwvb::access(pkt)->pk_num,NOW);


		      replyACK(pkt->copy());
		    }
		    else 
		      processPassive();
		}
	}
	Packet::free(pkt);
}

int UWALOHA::passBack(Packet* pkt,int sz){
  hdr_uwvb* vbh = HDR_UWVB(pkt);
  pos_info* pos = new pos_info[4];
  pos[0].x = vbh->info.ox;
  pos[0].y = vbh->info.oy;
  pos[0].z = vbh->info.oz;
  
  pos[1].x = vbh->info.fx;
  pos[1].y = vbh->info.fy;
  pos[1].z = vbh->info.fz;
  
  pos[2].x = vbh->info.tx;
  pos[2].y = vbh->info.ty;
  pos[2].z = vbh->info.tz;
  
  pos[3].x = vbh->info.fx + vbh->info.dx;
  pos[3].y = vbh->info.fy + vbh->info.dy;
  pos[3].z = vbh->info.fz + vbh->info.dz;

  pos_info src_pos = pos[0],fwd_pos = pos[1],tgt_pos = pos[2], my_pos = pos[3];
  double ox=src_pos.x,oy=src_pos.y,oz=src_pos.z;
  double tx=tgt_pos.x,ty=tgt_pos.y,tz=tgt_pos.z;
  double fx=fwd_pos.x,fy=fwd_pos.y,fz=fwd_pos.z;
  double x=my_pos.x,y=my_pos.y,z=my_pos.z;
  double prj_fwd_tgt,prj_my_tgt;
  //求解fwd在pipe上的投影
  double d_fwd_tgt=sqrt((tx-fx)*(tx-fx)+(ty-fy)*(ty-fy)+(tz-fz)*(tz-fz));
  double d_src_fwd=sqrt((fx-ox)*(fx-ox) + (fy-oy)*(fy-oy) +(fz-oz)*(fz-oz));
  double d_src_tgt=sqrt((tx-ox)*(tx-ox)+(ty-oy)*(ty-oy)+ (tz-oz)*(tz-oz));
  if(d_src_fwd==0)
    prj_fwd_tgt = d_src_tgt;
  else if(d_fwd_tgt==0)
    prj_fwd_tgt = d_fwd_tgt;
  else{
    double cos_fwd_tgt_src = (d_fwd_tgt*d_fwd_tgt + d_src_tgt*d_src_tgt - d_src_fwd*d_src_fwd)/(2*d_fwd_tgt*d_src_tgt);
    prj_fwd_tgt = d_fwd_tgt * cos_fwd_tgt_src;
  }
  //求解me在pipe上的投影
  double d_my_tgt =sqrt((tx-x)*(tx-x) + (ty-y)*(ty-y) + (tz-z)*(tz-z));
  double d_src_my =sqrt((x-ox)*(x-ox) + (y-oy)*(y-oy) + (z-oz)*(z-oz));
  if(d_src_my == 0){
    prj_my_tgt = d_src_tgt;
  }else if(d_my_tgt == 0){
    prj_my_tgt = d_my_tgt;
  }else if(d_src_tgt==0){
    prj_my_tgt = d_src_fwd;
  }else{
    double cos_my_tgt_src = (d_src_tgt*d_src_tgt + d_my_tgt*d_my_tgt - d_src_my*d_src_my)/(2*d_src_tgt*d_my_tgt);
    prj_my_tgt = d_my_tgt*cos_my_tgt_src;
  }
  printf("prj_my_tgt:%f ,prj_fwd_tgt:%f\n",prj_my_tgt,prj_fwd_tgt);
  return prj_my_tgt < prj_fwd_tgt;
}

//sendACK
void UWALOHA::replyACK(Packet *pkt){
	nsaddr_t Data_Sender = hdr_UWALOHA::access(pkt)->SA;
	/*start shaoyang*/
	Packet *ack_pkt = makeACK(Data_Sender,hdr_uwvb::access(pkt));
	/*end shaoyang*/
	
	sendPkt(ack_pkt);
	bo_counter=0;
	Packet::free(pkt);
}

Packet* UWALOHA::makeACK(nsaddr_t Data_Sender,const hdr_uwvb* vbh){
	Packet* pkt = Packet::alloc();
	hdr_cmn* cmh = HDR_CMN(pkt);
	hdr_UWALOHA* UWALOHAh = hdr_UWALOHA::access(pkt);
	
	cmh->size() = hdr_UWALOHA::size();		//将hdr_UWALOHA的size赋给cmh
	cmh->txtime() = getTxTime(10);			//默认ACK的packet_size为10
	/*	if(AckSizeRatio <= 1.0)
	  cmh->txtime() = getTxTime(cmh->size()/4.0);
	else
	  cmh->txtime() = getTxTime(cmh->size()/AckSizeRatio*1.0);*/
	cmh->error() = 0;
	cmh->direction() = hdr_cmn::DOWN;
	cmh->next_hop() = Data_Sender;
	cmh->ptype() = PT_UWALOHA;

	hdr_uwvb* vbh_hdr = HDR_UWVB(pkt);
	nexthop_elem* elem =  new nexthop_elem(index_);
	elem->residual_buffer = 10.0;
	//获取自身能量
	elem->residual_energy = em()->energy();
	elem->rtt = cmh->timestamp();
	elem->pos.x = vbh->info.fx + vbh->info.dx;
	elem->pos.y = vbh->info.fy + vbh->info.dy;
	elem->pos.z = vbh->info.fz + vbh->info.dz;
#ifdef CHANNEL_STATUS_EVALUATING
	assert(chan_status_table != NULL);
	assert(chan_status_table.size() > 0);
	double factor = chan_status_table->evaluate_chanl();
	elem->channel_status_factor = factor;
	UWALOHAh->chnl_status_factor = factor;
#endif
	printf("%f %f\n",vbh->info.fx,vbh->info.dx);
	vbh_hdr->mess_type = V_ACK;
	vbh_hdr->nexthop_ptr = elem;

	UWALOHAh->packet_type = hdr_UWALOHA::UW_ACK;
	UWALOHAh->SA = index_;
	UWALOHAh->DA = Data_Sender;
	UWALOHAh->ack_pkt_no = vbh->pk_num;
	UWALOHAh->residual_energy = em()->energy();
	
	return pkt;
}

void UWALOHA::processRetryTimer(UWALOHA_ACK_RetryTimer* timer){
	Packet* pkt = timer->pkt();
	if( RetryTimerMap_.count(timer->id()) != 0 ) {
		RetryTimerMap_.erase(timer->id());
	} else {
		printf("error: cannot find the ack_retry timer\n");
	}
	delete timer;
	sendPkt(pkt);
}

void UWALOHA::retryACK(Packet* ack){
	UWALOHA_ACK_RetryTimer* timer = new UWALOHA_ACK_RetryTimer(this, ack);
	timer->resched(MAXACKRetryInterval*Random::uniform());
	RetryTimerMap_[timer->id()] = timer;
}

int UWALOHA::command(int argc, const char *const *argv){
	return UnderwaterMac::command(argc, argv);
}
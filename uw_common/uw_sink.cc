
/********************************************************************/
/* sink for underwater                                              */
/********************************************************************/


#include <stdlib.h>
#include <tcl.h>
#include <stdio.h>

#include "uw_sink.h"
#include "agent.h"
#include "tclcl.h"
#include "random.h"


#define REPORT_PERIOD     2
#define INTEREST_PERIOD   2

bool operator<(const sense_area_elem&  e1, const sense_area_elem& e2)
{
	return (e1.sense_x<e2.sense_x)&&(e1.sense_y<e2.sense_y)
		&&(e1.sense_z<e2.sense_z)&&(e1.sense_r<e2.sense_r);
}

void SenseArea::insert(double x, double y, double z, double r)
{
	AreaSet.insert( sense_area_elem(x,y,z,r));
}

bool SenseArea::IsInSenseArea(double nx, double ny, double nz)
{
	double delta_x, delta_y, delta_z, distance;
	set<sense_area_elem>::iterator pos = AreaSet.begin();
	while( pos != AreaSet.end() ) {
		delta_x = nx - (*pos).sense_x;
		delta_y = ny - (*pos).sense_y;
		delta_z = nz - (*pos).sense_z;

		distance = sqrt(delta_x*delta_x+delta_y*delta_y+delta_z*delta_z);
		if( distance < (*pos).sense_r )
			return true;
		pos++;
	}
	return false;
}


void UWReport_Timer::expire(Event *) {
	a_->report();
}


void UWSink_Timer::expire(Event *) {
	a_->timeout(0);
}

void UWPeriodic_Timer::expire(Event *) {
	a_->bcast_interest();
}
static class UWSinkClass : public TclClass {
public:
	UWSinkClass() : TclClass("Agent/UWSink") {}
	TclObject* create(int , const char*const* ) {
		return(new UWSinkAgent());
	}
} class_uwsink;

int UWSinkAgent::pkt_id_ = 0;

UWSinkAgent::UWSinkAgent() : Agent(PT_UWVB),running_(0), random_(0), sink_timer_(this), periodic_timer_(this),
report_timer_(this)
{
	// set option first.
	APP_DUP_ = true; // allow duplication 
	periodic_ = false; //just send out interest once
	//always_max_rate_ = false; //?

	//Bind Tcl and C++ Variables
	bind("num_send",&num_send);
	//bind("num_recv",&num_recv);
	//bind("maxpkts_", &maxpkts_);

	//Initialize variables.

	maxpkts_ = 1000;
	pk_count=0;
	num_recv=0;
	num_send=0;
	//  RecvPerSec=0;
	strcpy(f_name,"test.data");
	target_x=0;
	target_y=0;
	target_z=0;
	target_id.addr_=0;
	target_id.port_=0; 



	passive=0;
	cum_delay=0.0;

	ActiveSense = 0;


	// bind("data_type_", &data_type_);
	bind_time("data_rate_", &data_rate_);
	bind("packetsize_", &packetsize_);
	bind("random_", &random_);
	bind("passive",&passive);
	bind("ActiveSense", &ActiveSense);
	bind("SenseInterval", &SenseInterval);
	// bind("num_recv", &num_recv);
	// bind("num_send", &num_send);
	// bind("cum_delay", &cum_delay);
	/* 
	size_=64;
	interval_=1;
	random_=0;
	*/
	data_counter = 0;
	//simple_report_rate = ORIGINAL;
	last_arrival_time = -1.0;
}

void UWSinkAgent::start()
{
	running_ = 1;
	interval_=1.0/data_rate_;
	random_=0;
	sendpkt();
	sink_timer_.resched(interval_);
}

void UWSinkAgent::exponential_start()
{
	random_=2;
	running_ = 1;	
	generateInterval();
	sink_timer_.resched(interval_);
}


void 
UWSinkAgent::generateInterval()
{
	double R=Random::uniform();
	double lambda=data_rate_;
	interval_=-log(R)/lambda;
	// printf("\nuwsink: !!!!!!!!generateInterval the  inetrval is %f\n",interval_);
	return; 
}


void UWSinkAgent::stop()
{
	if (running_) {
		running_ = 0;
	}

	if (periodic_ == true) {
		periodic_ = false;
		periodic_timer_.force_cancel();
	}
}


void UWSinkAgent::report()
{
	printf("SK %d:  at time %lf\n", here_.addr_, NOW);
	report_timer_.resched(REPORT_PERIOD);
	// RecvPerSec = 0;
}


void UWSinkAgent::timeout(int)
{
	if (running_) {
		sendpkt();
		double t = interval_;
		if (random_==1)
			/* add some zero-mean white noise */
			t += interval_ * Random::uniform(-0.5, 0.5);
		if(random_==2) {
			generateInterval();
			t=interval_;

		}
		sink_timer_.resched(t);
	}
}


void UWSinkAgent::sendpkt()
{
	if( ActiveSense ) {
		node->update_position();
		if( !SenseAreaSet.IsInSenseArea(node->X(), node->Y(), node->Z()) ) {
			//detect frequently
			interval_ = 1;
			return;
		}
		else {
			interval_ = SenseInterval;
		}
	}

	//add by Shaoyang
	/*
	set<neighbor_node_elem> neighbor_nodes =  node->neighbor_nodes.neighbor_node_set;
	set<neighbor_node_elem>::iterator it;
	neighbor_node_elem em;
	int next_hop_id;
	printf("%d\n",node->nodeid());

	if(!neighbor_nodes.empty()){		
		for(it=neighbor_nodes.begin();it!=neighbor_nodes.end();it++){
			neighbor_node_elem em = *it;
			printf("%d ",em.node_id);
		}
		printf("\n");
	}
	*/
	Packet* pkt = Packet::alloc();
	hdr_uwvb* vbh = HDR_UWVB(pkt);
	hdr_ip* iph = HDR_IP(pkt);
	hdr_cmn*  cmh = HDR_CMN(pkt);

	//added by Shaoyang,temporarily use the first as the next_hop
	
	cmh->ptype()=PT_UWVB;
	cmh->size() = packetsize_;
	printf("set packet size of agent:cc%d\n",cmh->size());
	cmh->uid() = pkt_id_++;
	vbh->mess_type = DATA;
	vbh->pk_num = pk_count;
	vbh->ts_=NOW;
	pk_count++;
	iph->src_=here_;
	iph->dst_.addr_=here_.addr_;
	iph->dst_.port_=255;

	vbh->sender_id = here_;
	// vbh->data_type = data_type_;
	vbh->forward_agent_id = here_; 
	vbh->target_id=target_id;
	vbh->range=range_;
	vbh->info.tx=target_x;
	vbh->info.ty=target_y; 
	vbh->info.tz=target_z;
	vbh->info.fx=node->CX();
	vbh->info.fy=node->CY();
	vbh->info.fz=node->CZ();
	vbh->info.ox=node->CX();
	vbh->info.oy=node->CY(); 
	vbh->info.oz=node->CZ();
	vbh->info.dx=0;
	vbh->info.dy=0; 
	vbh->info.dz=0;

	/*     
	vbh->original_source.x=vbh->info.fx;
	vbh->original_source.y=vbh->info.fy;
	vbh->original_source.z=vbh->info.fz;
	*/

	//printf("uw_sink:source(%d,%d) send packet %d at %lf : the coordinates of target is (%lf,%lf,%lf) and range=%lf and my position (%f,%f,%f) and cx is(%f,%f,%f)  type is %d\n", vbh->sender_id.addr_,vbh->sender_id.port_,vbh->pk_num, NOW, vbh->info.tx=target_x,vbh->info.ty=target_y, vbh->info.tz=target_z,vbh->range,node->X(),node->Y(),node->Z(),node->CX(),node->CY(),node->CZ(),vbh->mess_type);
	num_send++;
	//   vbh->attr[0] = data_type_;

	if (target_==NULL)  printf("The target_ is empty\n");
	else { 
		// printf("The target_ is not  empty\n");
		send(pkt, 0);
	}
	// printf("I exit the sendpk \n");
}


void UWSinkAgent::data_ready()
{

	// printf("I am in the sendpk1 \n");
	if (pk_count >=  maxpkts_) {
		running_ = 0;
		return;
	}
	// printf("I am in the sendpk2 \n");
	Packet* pkt = create_packet();
	hdr_uwvb* vbh = HDR_UWVB(pkt);
	hdr_ip* iph = HDR_IP(pkt);
	//hdr_cmn*  cmh = HDR_CMN(pkt);

	vbh->mess_type = DATA_READY;
	vbh->pk_num = pk_count;
	pk_count++;
	iph->src_=here_;
	iph->dst_.addr_=here_.addr_;
	iph->dst_.port_=255;
	vbh->sender_id = here_;
	//      vbh->data_type = data_type_;
	vbh->forward_agent_id = here_; 
	vbh->target_id=target_id;
	vbh->range=range_;
	vbh->ts_=NOW;

	printf("uw_sink:source(%d,%d)(%f,%f,%f) send data ready packet %d at %lf : the target is (%d,%d)\n", vbh->sender_id.addr_,vbh->sender_id.port_,node->X(),node->Y(),node->Z(),vbh->pk_num, NOW, vbh->target_id.addr_,vbh->target_id.port_);


	// Send the packet
	// printf("Source %s send packet ( %x,%d) at %lf.\n", name(), 
	// vbh->sender_id, vbh->pk_num, NOW);


	//  num_send++;
	//      vbh->attr[0] = data_type_;
	if (target_==NULL)  printf("The target_ is empty\n");
	else {
		// printf("The target_ is not  empty\n");
		send(pkt, 0);
	}
	// printf("I exit the sendpk \n");
}



void UWSinkAgent::source_deny(ns_addr_t id, double x, double y, double z)
{


	Packet* pkt = create_packet();
	hdr_uwvb* vbh = HDR_UWVB(pkt);
	hdr_ip* iph = HDR_IP(pkt);

	// Set message type, packet number and sender ID
	vbh->mess_type = SOURCE_DENY;
	vbh->pk_num = pk_count;
	pk_count++;

	iph->src_=here_;
	iph->dst_.addr_=here_.addr_;
	iph->dst_.port_=255;
	vbh->sender_id = here_;	
	//vbh->data_type = data_type_;
	vbh->forward_agent_id = here_; 
	vbh->target_id=id;
	vbh->info.tx=x;
	vbh->info.ty=y; 
	vbh->info.tz=z;
	vbh->range=range_;
	vbh->ts_=NOW;

	// Send the packet
	vbh->info.ox=node->X();
	vbh->info.oy=node->Y(); 
	vbh->info.oz=node->Z();
	/* 
	vbh->original_source.x=vbh->info.ox;
	vbh->original_source.y=vbh->info.oy;
	vbh->original_source.z=vbh->info.oz;
	*/
	vbh->info.dx=0;
	vbh->info.dy=0; 
	vbh->info.dz=0;

	printf("uw_sink:source(%d,%d) send source-deny packet %d at %lf : the target is (%d,%d)\n", vbh->sender_id.addr_,vbh->sender_id.port_,vbh->pk_num, NOW, vbh->target_id.addr_,vbh->target_id.port_);
	send(pkt, 0);	
}



void UWSinkAgent::bcast_interest()
{

	Packet* pkt = create_packet();
	// printf("uw_sink: the address of thenew packet is%d\n",pkt);
	hdr_uwvb* vbh = HDR_UWVB(pkt);
	hdr_ip* iph = HDR_IP(pkt);

	// Set message type, packet number and sender ID
	iph->src_=here_;
	iph->dst_.addr_=here_.addr_;
	iph->dst_.port_=255;

	vbh->mess_type = INTEREST;
	vbh->pk_num = pk_count;
	pk_count++;
	vbh->sender_id = here_;	
	// vbh->data_type = data_type_;
	vbh->forward_agent_id = here_; 
	vbh->target_id=target_id;
	vbh->info.tx=target_x;
	vbh->info.ty=target_y; 
	vbh->info.tz=target_z;
	vbh->range=range_;
	vbh->ts_=NOW;
	
	// Send the packet
	vbh->info.ox=node->X();
	vbh->info.oy=node->Y();
	vbh->info.oz=node->Z();

	/*
	vbh->original_source.x=vbh->info.ox;
	vbh->original_source.y=vbh->info.oy;
	vbh->original_source.z=vbh->info.oz;
	*/

	vbh->info.dx=0;
	vbh->info.dy=0; 
	vbh->info.dz=0;



	printf("uw_sink:source(%d,%d) send interest packet %d at %lf : the target is (%d,%d) coordinate is (%f,%f,%f)\n", vbh->sender_id.addr_,vbh->sender_id.port_,vbh->pk_num, NOW, vbh->target_id.addr_,vbh->target_id.port_,vbh->info.tx,vbh->info.ty, vbh->info.tz);



	// printf("Sink %s send packet (%x, %d) at %f to %x.\n", 
	//    name_, dfh->sender_id,
	//     dfh->pk_num, 
	//     NOW,
	//     iph->dst_);
	// printf("uw_sink:I am in the sendpk before send\n") ;

	send(pkt, 0);	 

	if (periodic_ == true)
		periodic_timer_.resched(INTEREST_PERIOD);
}

void UWSinkAgent::Terminate () 
{
	FILE * fp;
	if ((fp=fopen(f_name,"a"))==NULL){
		printf("SINK %d can not open file\n", here_.addr_);
		return;
	}

#ifdef DEBUG_OUTPUT
	printf("SINK(%d): terminates (send %d, recv %d, cum_delay %f)\n", 
		here_.addr_, num_send, num_recv, cum_delay);
#endif
	fprintf(fp,"SINK(%d) : pkt_count = %d,num_send = %d, num_recv = %d, cum_delay = %f\n",
		pk_count,here_.addr_, num_send, num_recv, cum_delay);

	printf("SINK %d : terminates (pkt_count:%d,send %d, recv %d, cum_delay %f)\n", 
		pk_count, here_.addr_, num_send, num_recv, cum_delay);
	int index=DataTable.current_index;

	for(int i=0;i<index;i++){
		//fprintf(fp,"SINK(%d) : send_id = %d, num_recv = %d\n", 
		//	here_.addr_, DataTable.node(i), DataTable.number(i));
		printf("SINK(%d) : send_id = %d, num_recv = %d\n", 
			here_.addr_, DataTable.node(i), DataTable.number(i));
	}

	fclose(fp);
	running_=0;
}


int UWSinkAgent::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
	if (argc == 2) {

		if (strcmp(argv[1], "enable-duplicate") == 0) {
			APP_DUP_ = true;
			return TCL_OK;
		}

		if (strcmp(argv[1], "disable-duplicate") == 0) {
			APP_DUP_ = false;
			return TCL_OK;
		}

		if (strcmp(argv[1], "always-max-rate") == 0) {
			//      always_max_rate_ = true;
			return TCL_OK;
		}

		if (strcmp(argv[1], "terminate") == 0) {
			Terminate();
			return TCL_OK; 

		}

		if (strcmp(argv[1], "announce") == 0) {
			bcast_interest();
			// report_timer_.resched(REPORT_PERIOD);
			return (TCL_OK);
		}

		if (strcmp(argv[1], "ready") == 0) {
			//    God::instance()->data_pkt_size = size_;
			data_ready();
			return (TCL_OK);
		}

		if (strcmp(argv[1], "send") == 0) {
			printf("before I am going into sendpk\n");
			sendpkt();   
			return (TCL_OK);
		}

		if (strcmp(argv[1], "cbr-start") == 0) {
			start();
			return (TCL_OK);
		}

		if (strcmp(argv[1], "exp-start") == 0) {
			exponential_start();
			return (TCL_OK);
		}

		if (strcmp(argv[1], "stop") == 0) {
			stop();
			report_timer_.force_cancel();
			return (TCL_OK);
		}

	}

	if (argc == 3) {

		if (strcmp(argv[1], "setTargetAddress") == 0) {
			target_id.addr_=atoi(argv[2]);
			return TCL_OK;
		}

		if (strcmp(argv[1], "set-target-x") == 0) {
			target_x=atoi(argv[2]);
			return TCL_OK;
		}

		if (strcmp(argv[1], "set-target-y") == 0) {
			target_y=atoi(argv[2]);
			return TCL_OK;
		}

		if (strcmp(argv[1], "set-target-z") == 0) {
			target_z=atoi(argv[2]);
			return TCL_OK;
		}

		if (strcmp(argv[1], "set-range") == 0) {
			// printf("I am set range part\n"); 
			range_=atoi(argv[2]);

			return TCL_OK;
		}
		if (strcmp(argv[1], "set-packetsize") == 0) {
			// printf("I am set range part\n"); 
			packetsize_=atoi(argv[2]);

			return TCL_OK;
		}

		if (strcmp(argv[1], "set-filename") == 0) {
			// printf("I am set the filename to %s \n",argv[2]);
			// printf("Old filename is %s \n",f_name); 
			strcpy(f_name,argv[2]);
			//  printf("Now  the filename is %s \n",f_name); 
			return TCL_OK;
		}
		/*
		if (strcmp(argv[1], "data-type") == 0) {
		data_type_ = atoi(argv[2]);
		return (TCL_OK);
		}
		*/ 

		if (strcmp(argv[1], "on-node") == 0) {    
			node= (UnderwaterSensorNode*) tcl.lookup(argv[2]); 
			if(node==NULL)printf("uw_sink: node is empty\n"); 
			return TCL_OK;
		}
		if (strcmp(argv[1], "attach-rt-agent") == 0) {
			//    printf("uw_sink:attach-rt-agent is called \n");
			TclObject *obj;

			if ( (obj = TclObject::lookup(argv[2])) == 0) {
				fprintf(stderr, "VSink node Node:: lookup  failed\n");
				return TCL_ERROR;
			}
			target_ = (NsObject *) obj;
			return TCL_OK;
		}
	}

	if( argc == 6 ) {
		if ( strcmp(argv[1], "set-sense-area") == 0 ) {
			SenseAreaSet.insert(atof(argv[2]), atof(argv[3]), atof(argv[4]), atof(argv[5]));
			return TCL_OK;
		}
	}
	return (Agent::command(argc, argv));
}


void UWSinkAgent::recv(Packet* pkt, Handler*)
{
	hdr_uwvb* vbh = HDR_UWVB(pkt);


	// printf("SK %d recv (%x, %x, %d) %s size %d at time %lf\n", here_.addr_, 
	// (dfh->sender_id).addr_, (dfh->sender_id).port_,
	// dfh->pk_num, MsgStr[dfh->mess_type], cmh->size(), NOW);

	/*
	if (data_type_ != vbh->data_type) {
	printf("SINK: Hey, What are you doing? I am not a sink for %d. I'm a sink for %d. \n", vbh->data_type, data_type_);
	Packet::free(pkt);
	return;
	}
	*/

	// printf("UWSink: I get the packet  \n");
	switch(vbh->mess_type) {
		/*
		case DATA_REQUEST :

		if (always_max_rate_ == false)
		simple_report_rate = vbh->report_rate;

		if (!running_) start();

		//      printf("I got a data request for data rate %d at %lf. Will send it right away.\n",
		//	     simple_report_rate, NOW);

		break;


		case DATA_STOP :

		if (running_) stop();
		break;
		*/
	case DATA_READY :
		printf("UWSink (id:%d)(%f,%f,%f): I get the data ready packet no.%d  \n",here_.addr_,node->CX(),node->CY(),node->CZ(), vbh->pk_num);

		if(node->sinkStatus()){
			passive=1;
			// num_recv++;    
			target_x=node->X()-node->CX();
			target_y=node->Y()-node->CY();
			target_z=node->Z()-node->CZ(); 
			last_arrival_time = NOW;
			target_id=vbh->sender_id;
			bcast_interest();
		}
		else{

			target_x=0;
			target_y=0;
			target_z=0; 
			target_id=vbh->sender_id;
			start();

		}
		break;





	case INTEREST:

		num_recv++;    
		target_id=vbh->sender_id;
		target_x=vbh->info.ox;
		target_y=vbh->info.oy;
		target_z=vbh->info.oz;
		running_=1;

		start();

		break;

	case DATA :
	case TARGET_DISCOVERY:
		{    //printf("uw_sink: the source is out of scope %d\n",passive);

			if(!passive){
				if(IsDeviation()) 
				{
					printf("uw_sink: the source is out of scope\n");
					double x=node->X()-node->CX();
					double y=node->Y()-node->CY();
					double z=node->Z()-node->CZ(); 
					ns_addr_t id=vbh->sender_id;
					source_deny(id,x,y,z);
					bcast_interest();   
				}
			}

			printf("UWSink (id:%d): I get the packet data no.%d from %d \n",here_.addr_, vbh->pk_num,vbh->forward_agent_id.addr_);       
			cum_delay = cum_delay + (NOW - vbh->ts_);
			num_recv++;
			// RecvPerSec++;
			//      God::instance()->IncrRecv();

			//        int* sender_addr=new int[1];
			// sender_addr[0]=vbh->sender_id.addr_;

			int sender_addr=vbh->sender_id.addr_;

			DataTable.PutInHash(sender_addr);

			if (last_arrival_time > 0.0) {
				printf("SK %d: Num_Recv %d, InterArrival %lf\n", here_.addr_, 
					num_recv, (NOW)-last_arrival_time);
			}

			last_arrival_time = NOW;

			break;
		}  
	default:

		break;
	}

	Packet::free(pkt);
}


bool UWSinkAgent::IsDeviation(){
	double dx=node->CX()-node->X();
	double dy=node->CY()-node->Y();
	double dz=node->CZ()-node->Z();
	if(sqrt((dx*dx)+(dy*dy)+(dz*dz))<range_) return false;
	return true;

}





void UWSinkAgent::reset()
{
}


void UWSinkAgent:: set_addr(ns_addr_t address)
{
	here_=address;
}


int UWSinkAgent:: get_pk_count()
{
	return pk_count;
}


void UWSinkAgent:: incr_pk_count()
{
	pk_count++;
}  

Packet * UWSinkAgent:: create_packet()
{
	Packet *pkt = allocpkt();

	if (pkt==NULL) return NULL;

	hdr_cmn*  cmh = HDR_CMN(pkt);

	cmh->size() = 0;  // the control packet size is determined by the vbf header
	cmh->ptype()=PT_UWVB;
	hdr_uwvb* vbh = HDR_UWVB(pkt);
	vbh->ts_ = NOW; 
	return pkt;
}




#include "myping.h"

static class MyPingHeaderClass : public PacketHeaderClass{
    public:
        MyPingHeaderClass() : PacketHeaderClass("PacketHeader/Myping",sizeof(hdr_myping)){}
}class_mypinghdr;

static class MyPingClass : public TclClass{
    public:
        MyPingClass() : TclClass("Agent/Myping"){}
        TclObject* create(int ,const char*const*){
            return (new MyPingAgent());
        }
}class_myping;

//PingAgent constructor
MyPingAgent::MyPingAgent() : Agent(PT_MYPING){
    bind("packetSize_",&size_);
    //bind("off_ping_",&off_ping_);
    //bind("off_ip_",&off_ip_);
}

int MyPingAgent::command(int argc,const char*const* argv){
    if( 2 == argc ){
        if(strcmp(argv[1],"send") == 0){
            //create a new packet
            Packet *pkt = allocpkt();
            //access the ping header for the new header
            hdr_myping *hdr = (hdr_myping*)pkt->access(off_ping_);
            //set the 'ret' field to 0,so the receiving node knows
            //that it has to generate an echo packet
            hdr->ret = 0;
            //store the current time in 'send_time' field
            hdr->send_time = Scheduler::instance().clock();
            //send the packet
            send(pkt,0);
            //return TCL_OK,so the calling function knows that
            //the command has been processed
            return (TCL_OK);
        }
    }
    //if the command has not been processed by PingAgent()::command,
    //call the command() function for the base class
    return (Agent::command(argc,argv));
}

void MyPingAgent::recv(Packet* pkt, Handler*){
    //acess the Ping header for the received packet
    hdr_myping* hdr = (hdr_myping*)pkt->access(off_ping_);
    //access the ip header for the received packet
    off_ip_ = hdr_ip::offset();
    hdr_ip* hdrip = (hdr_ip*)pkt->access(off_ip_);
    //is the 'ret' field = 0(the receiving node is being pinged)
    if(hdr->ret ==0){
        //send an 'echo'. save the old packet's send_time
        double stime = hdr->send_time;
        //discard the packet
        Packet::free(pkt);
        //create a new packet
        Packet* pktret = allocpkt();
        //acess the Ping header for the new packet
        hdr_myping* hdrret = (hdr_myping*)pktret->access(off_ping_);
        //set the 'ret' field to 1,so the receiver won't send anothe
        hdrret->ret = 1;
        //set the send_time field to the correct value
        hdrret->send_time = stime;
        send(pktret,0);
    }else{
        //use tcl.eval to call the Tcl interpreter
        char out[100];
        sprintf(out,"%s recv %d %3.1f", name(), hdrip->src_.addr_ >> Address::instance().NodeShift_[1],(Scheduler::instance().clock()-hdr->send_time)*1000);
        Tcl& tcl = Tcl::instance();
        tcl.eval(out);
        //discard the packet
        Packet::free(pkt);
    }
}

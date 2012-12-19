/*
 * File: Header File for a new 'Ping' Agent Class for the ns
 *       network simulator
 * Author: Marc Greis (greis@cs.uni-bonn.de), May 1998
 *
 */


#ifndef ns_ping_h
#define ns_ping_h

#include "agent.h"
#include "tclcl.h"
#include "packet.h"
#include "address.h"
#include "ip.h"


struct hdr_myping {
  char ret;
  double send_time;
  static int offset_;
  inline static hdr_myping* access(const Packet* p){
    return (hdr_myping*)p->access(offset_);
  }
};


class MyPingAgent : public Agent {
    public:
        MyPingAgent();
        int command(int argc, const char*const* argv);
        void recv(Packet*, Handler*);
    protected:
        int off_ping_;
        int off_ip_;
        //int packetSize_;
};


#endif

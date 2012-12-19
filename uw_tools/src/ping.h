#ifndef ns_ping_h
#define ns_ping_h

#include "../common/agent.h"
#include "../../tclcl-1.19/tclcl.h"
#include "../common/packet.h"
#include "../routing/address.h"
#include "../common/ip.h"

struct hdr_ping{
    char ret;
    double send_time;
};


class PingAgent : public Agent{
    public:
        PingAgent();
        int command(int argc,const char* const* argv);
        void recv(Packet* ,Handler*);
    protected:
        int off_ping_;
}

#endif


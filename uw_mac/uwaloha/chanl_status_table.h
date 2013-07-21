#ifndef CHANL_STATUS_TABLE_H
#define CHANL_STATUS_TABLE_H

#include <vector>
using namespace std;
const static int MAX_CHANL_STATUS_SIZE = 30;

struct chanl_status{
    int target_id;
    int pkt_id;
    int is_acked;
    double send_time;
    double recv_time;
    double ack_time;
    int retry_times;

    chanl_status(int targetid=0,int pktid=0,int isacked=0):target_id(targetid),
                pkt_id(pktid),is_acked(isacked),send_time(0),recv_time(0),ack_time(0),retry_times(0){}

    chanl_status(const chanl_status& another){
        target_id = another.target_id;
        pkt_id = another.pkt_id;
        send_time = another.send_time;
        recv_time = another.recv_time;
        ack_time = another.ack_time;
        retry_times = another.retry_times;
	is_acked = another.is_acked;
    }
};

struct chanl_statistic{
  int nexthop_id;
  int retry_times;
  bool success;
  chanl_statistic(int x,int y,bool z):nexthop_id(x),
	  retry_times(y),success(z){}
};

static chanl_status empty(-1,-1);

class Chanl_Status_Table{
    public:
        Chanl_Status_Table(int);
        void put_into_table(const chanl_status&);
        vector<chanl_status> vtable(){return table;}
        void print_all_elements();
        chanl_status* find_elem(const chanl_status&);
        void delete_elems(const chanl_status &elem);
        chanl_status* ack_elem(const chanl_status &elem);
        chanl_status* retry_elem(const chanl_status &elem);
	double evaluate_chanl();
    private:
        vector<chanl_status> table;
};

#endif

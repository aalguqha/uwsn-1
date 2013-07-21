#ifndef __UWALOHA_H__
#define __UWALOHA_H__

#include <packet.h>
#include <random.h>
#include <queue>
#include <map>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <timer-handler.h>
#include <mac.h>
#include "../underwatermac.h"
#include "../underwaterchannel.h"
#include "../underwaterpropagation.h"
#include "../../uw_common/uw_tools.h"
#include "../../uw_routing/vectorbasedforward.h"
#include "chanl_status_table.h"

typedef double Time;
#define CALLBACK_DELAY 0.001	//the interval between two consecutive sendings
//#define MAXIMUMCOUNTER 3
const int MAXIMUMCOUNTER = 3;
class UWALOHA;

typedef struct{
  double x;
  double y;
  double z;
} pos_info;


struct hdr_UWALOHA{
	nsaddr_t SA;
	nsaddr_t DA;
	int ack_pkt_no;
	double residual_energy;
	double chnl_status_factor;
	pos_info instant_pos;
	
	enum PacketType {
		UW_DATA,
		UW_ACK
	} packet_type;
	
	static int offset_;
	
	inline static int& offset() {  return offset_; }
	
	inline static int size() {
		return sizeof(nsaddr_t)*2 +sizeof(int)+2*sizeof(double) + sizeof(pos_info)+1; /*for packet_type*/
	}

	inline static hdr_UWALOHA* access(const Packet*  p) {
		return (hdr_UWALOHA*) p->access(offset_);
	}

};

class UWALOHA_DataSendTimer: public TimerHandler{
  public:
	UWALOHA_DataSendTimer(UWALOHA* mac): TimerHandler() {
		mac_ = mac;
	}
	//void resched(double delay);
	Packet* pkt_;

  protected:
	UWALOHA* mac_;
	virtual void expire(Event *e);
};

class UWALOHA_WaitACKTimer: public TimerHandler{
  public:
	UWALOHA_WaitACKTimer(UWALOHA* mac): TimerHandler() {
		mac_ = mac;
		ack_times_ = 1;
	}
	//void resched(double delay);

  protected:
	UWALOHA* mac_;
	int	 ack_times_;
	virtual void expire(Event* e);
};


class UWALOHA_BackoffTimer: public TimerHandler{
  public:
	UWALOHA_BackoffTimer(UWALOHA* mac): TimerHandler() {
		mac_ = mac;
	}
	//void resched(double delay);

  protected:
	UWALOHA* mac_;
	virtual void expire(Event* e);
};


class UWALOHA_ACK_RetryTimer: public TimerHandler{
  public:
	UWALOHA_ACK_RetryTimer(UWALOHA* mac, Packet* pkt=NULL) {
		mac_ = mac;
		pkt_ = pkt;
		id_ = id_generator++;
	}
	
	Packet*& pkt() {
		return pkt_;
	}
	
	long id() {
		return id_;
	}
	
  protected:
	UWALOHA* mac_;
	Packet* pkt_;
	long	id_;
	static long id_generator;
	virtual void expire(Event* e);
};

class UWALOHA_StatusHandler: public Handler{
  public:
	UWALOHA_StatusHandler(UWALOHA* mac): Handler() {
		mac_ = mac;
	}
	bool&	is_ack() {
		return is_ack_;
	}
  protected:
	UWALOHA* mac_;
	bool	is_ack_;
	virtual void handle(Event* e);
};

class UWALOHA_CallBackHandler: public Handler{
  public:
	UWALOHA_CallBackHandler(UWALOHA* mac): Handler() {
		mac_ = mac;
	}
  protected:
	UWALOHA* mac_;
	virtual void handle(Event* e);
};


class UWALOHA: public UnderwaterMac{
public:
	UWALOHA();
	int  command(int argc, const char*const* argv);
	void TxProcess(Packet* pkt);
	void RecvProcess(Packet* pkt);
	int bo_counter;
	
	void	processRetryTimer(UWALOHA_ACK_RetryTimer* timer);
protected:
	enum {
		PASSIVE,
		BACKOFF,
		SEND_DATA,
		WAIT_ACK,
	}UWALOHA_Status;

	EnergyModel* em() { return node()->energy_model(); }

	double	Persistent;
	int ACKOn;
	double AckSizeRatio;
	Time	Min_Backoff;
	Time 	Max_Backoff;
	Time	WaitACKTime;
	Time	MAXACKRetryInterval;
	
	bool	blocked;
	int	seq_n;
	
	UWALOHA_BackoffTimer		BackoffTimer;
	UWALOHA_WaitACKTimer		WaitACKTimer;

	UWALOHA_CallBackHandler		CallBack_handler;
	UWALOHA_StatusHandler		status_handler;

	Time MaxPropDelay;
	Time DataTxTime;
	Time ACKTxTime;


	queue<Packet*>	PktQ_;
	map<long, UWALOHA_ACK_RetryTimer*> RetryTimerMap_;   //map timer id to the corresponding pointer
	/*shaoyang*/
	Packet* makeACK(nsaddr_t,const hdr_uwvb*);	
	int passBack(Packet* pkt,int sz);
	int WAIT_ACK_NODE;
	UWTools uw_tools;
	Chanl_Status_Table* chan_status_table;
	
	/*shaoyang*/
	Event	status_event;
	Event 	Forward_event;
	Event	callback_event;
	void	replyACK(Packet* pkt);
	void	sendACK(Time DeltaTime);
	void	sendPkt(Packet* pkt);//why?
	void	sendDataPkt();
//	void 	DropPacket(Packet*);
	void	doBackoff();
//	void	processDataSendTimer(Event *e);
	void	processWaitACKTimer(Event *e);
	void	processPassive();
//	void	processBackoffTimer();
	
	void	retryACK(Packet* ack);
	void	StatusProcess(bool is_ack);
	void	BackoffProcess();
	void	CallbackProcess(Event* e);

	bool	CarrierDected();
//	void	doBackoff(Event *e);
	/*start shaoyang*/
	
	friend class UWALOHA_BackoffTimer;
	friend class UWALOHA_WaitACKTimer;
	friend class UWALOHA_CallBackHandler;
	friend class UWALOHA_StatusHandler;
	friend class UWALOHA_ForwardHandler;
};

#endif



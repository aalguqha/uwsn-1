#ifndef __ns_underwaterchannel_h__
#define __ns_underwaterchannel_h__

#include <string.h>
#include <mac/channel.h>
#include "object.h"
#include "packet.h"
#include "phy.h"
#include "node.h"

class UnderwaterChannel : public Channel {
	friend class Topography;
public:
	UnderwaterChannel(void);
	virtual int command(int argc, const char*const* argv);
        static double Transmit_distance(){return distCST_;}; 
private:
	void sendUp(Packet* p, Phy *txif);
	double get_pdelay(Node* tnode, Node* rnode);
	
	/* For list-keeper, channel keeps list of mobilenodes 
	   listening on to it */
	int numNodes_;
	MobileNode *xListHead_;
	bool sorted_;
        void calculatePosition(Node* sender,Node* receiver, Packet* p);
        double distance(Node* ,Node*);
	void addNodeToList(MobileNode *mn);
	void removeNodeFromList(MobileNode *mn);
	void sortLists(void);
	void updateNodesList(class MobileNode *mn, double oldX);
	MobileNode **getAffectedNodes(MobileNode *mn, double radius, int *numAffectedNodes);
       
	
protected:
	//double distCST_;
	static double distCST_;
};

#endif //_underwaterchannel_h_

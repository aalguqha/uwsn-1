ns 2_aloha.tcl > res1.txt
grep "SKGD" res1.txt  > chnl_sen_pkt.txt
#grep "node0 recv pkt:" res1.txt  > chnl_sen_pkt.txt
#grep "I recv an ACK (index,SA),WAIT_ACK_NODE" res1.txt  >> chnl_sen_pkt.txt
#grep "backoffhandler: too many backoffs,decide re-transmission" res1.txt  >> chnl_sen_pkt.txt

tail -10 res1.txt
cat /dev/null > res1.txt

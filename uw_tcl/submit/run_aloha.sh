ns 3_bcst.tcl > res1.txt

grep "phy decrease energy:" res1.txt  > chnl_sen_pkt.txt
#grep "node0 recv pkt:" res1.txt  > chnl_sen_pkt.txt
#grep "I recv an ACK (index,SA),WAIT_ACK_NODE" res1.txt  >> chnl_sen_pkt.txt
#grep "backoffhandler: too many backoffs,decide re-transmission" res1.txt  >> chnl_sen_pkt.txt

tail -10 res1.txt
cat /dev/null > res1.txt

#awk -f data/delay.awk data/3_aloha.tr
#awk -f data/throughput.awk data/3_aloha.tr

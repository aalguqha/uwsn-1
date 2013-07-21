./ns_aloha 3_aloha.tcl > res1.txt
./ns_bcst 3_bcst.tcl > res2.txt

grep "SINK(0) : send_id = 6" res1.txt > chnl_sen_pkt.txt
grep "god: the energy consumped" res1.txt >> chnl_sen_pkt.txt
grep "SINK(0) : send_id = 6" res2.txt >> chnl_sen_pkt.txt
grep "god: the energy consumped" res2.txt >> chnl_sen_pkt.txt

#grep "node0 recv pkt:" res1.txt  > chnl_sen_pkt.txt
#grep "I recv an ACK (index,SA),WAIT_ACK_NODE" res1.txt  >> chnl_sen_pkt.txt
#grep "backoffhandler: too many backoffs,decide re-transmission" res1.txt  >> chnl_sen_pkt.txt


echo "aloha"
echo "bcst"
tail -12 chnl_sen_pkt.txt
cat /dev/null > res2.txt
cat /dev/null > res1.txt
cat /dev/null > chnl_sen_pkt.txt
#awk -f data/delay.awk data/3_bcst.tr
#awk -f data/throughput.awk data/3_bcst.tr

#!/bin/bash
ns 2_aloha.tcl > res1.txt
grep "node0 recv pkt" res1.txt  > chnl_sen_pkt.txt
grep "node0 discard pkt" res1.txt  > chnl_sen_pkt2.txt

tail -10 res1.txt
cat /dev/null > res1.txt

a=`cat chnl_sen_pkt.txt | wc -l`
b=`cat chnl_sen_pkt2.txt | wc -l`
c=`echo "scale=2;$b/$a" | bc`
echo $a,$b,$c

#cp /home/villasy/Desktop/aqua-sim2/ns-allinone-2.30/ns-2.30/ns ns_bcst
./ns_vbf 3_bcst.tcl > res2.txt

#cp /home/villasy/Desktop/aqua-sim2/ns-allinone-2.30/ns-2.30/ns ns_aloha
./ns_mvbf 3_aloha.tcl > res1.txt

awk -f data/throughput.awk data/3_aloha.tr
awk -f data/th.awk data/3_bcst.tr


./ns_vbf 3_bcst.tcl > res2.txt
./ns_mvbf 3_aloha.tcl > res1.txt

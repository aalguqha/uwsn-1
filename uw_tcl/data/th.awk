BEGIN{
  src_node = 6;
  sink_node = 0;
  i=0;
  highest_pkt_num = 0;
}
{
  if(NF > 7){
    event = $1;
    time = $3;
    this_node = $5;
    next_hop = $7;
    pos_x = $11;
    pos_y = $13;
    pos_z = $15;
    energy = $17;
    trace_level = $19;
    reason = $21;
    pkt_src_id = $31;
    pkt_dest_id = $33;
    pkt_type = $35;
    pkt_size = $37;
    flow_id = $39;
    pkt_num = $41;
    ttl = $43;
	#统计发送初始时间
    if( this_node == src_node && i==0){
        start_time = time;
	i = i+1;
    }
    if(this_node == sink_node && pkt_type == "vectorbasedforward"){
      if(event=="r"){
	  if(pkt_num > highest_pkt_num)
	    highest_pkt_num = pkt_num;
	  if(pkt_bytes[pkt_num] < 0.000001){
	      recv_time[pkt_num] = time;
	  }
	  pkt_bytes[pkt_num] += pkt_size;
      }else if(event=="d"){
	  pkt_bytes[pkt_num] -= 1*pkt_size;
      }
    }
  }
}
END{
    sum = 0;
    average = 0;
    count = 0;
    #printf("pkt_num:%d\n",highest_pkt_num);
    for(j=0;j < highest_pkt_num;++j){
      duration = recv_time[j] - start_time;
      #printf("pkt_num:%d,time:%f,throughput:%f Bps\n",j,duration,pkt_bytes[j]);
      if(duration > 0){
	count++;
	sum += pkt_bytes[j];
	th = sum/duration;
	average += th;
	#printf("pkt_num:%d,start_time:%f,time:%f,throughput:%f Bps\n",j,start_time,recv_time[j],th);
      }
  }
  printf("broadcast:%f Bps\n",average/count);
}

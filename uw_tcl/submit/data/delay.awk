BEGIN{
  src_node = 6;
  sink_node = 0;
  init = 0;
  total_num = 0;
  max_pkt_num =0;
  j = 0;
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
    
    if(pkt_num > total_num)
      total_num = pkt_num;
    if( event=="r" && pkt_type == "vectorbasedforward" && pkt_src_id == src_node){
      if(pkt_num > max_pkt_num){
	max_pkt_num = pkt_num;
      }
    #printf("%s %.9f %d %d %d %d %d\n",event,time,this_node,pkt_src_id,pkt_dest_id,pkt_type,pkt_num);
      if(this_node == src_node ){
	#printf("send:%s %.9f this:%d,pkt_src:%d,pkt_dest:%d,pkt_type:%s,pkt_num:%d\n",
	    #event,time,this_node,pkt_src_id,pkt_dest_id,pkt_type,pkt_num);
	start_time[pkt_num] = time;
      }else if(this_node == sink_node){
	#printf("recv:%s %.9f this:%d,pkt_src:%d,pkt_dest:%d,pkt_type:%s,pkt_num:%d\n",
		#event,time,this_node,pkt_src_id,pkt_dest_id,pkt_type,pkt_num);
	if(end_time[pkt_num] <= 0)
	  end_time[pkt_num] = time;
      }else{
	end_time[pkt_num] = -1;
      }
    }
  }
}
END{
  #printf("biggest_pkt_num:%d\n",max_pkt_num);
  #统计成功接收的数据包的延时
  for(i=0;i < max_pkt_num;++i){
    duration = end_time[i] - start_time[i];
    if(duration > 0)
      printf("pkt_num:%d,delay:%f ms\n",i,duration);
  }
}

BEGIN{
  src_node = 6;
  sink_node = 0;
  max_pkt_num = 20;
  i=0;
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
    #&& pkt_num < max_pkt_num
    if( event=="r" && pkt_type == "vectorbasedforward" ){
	#统计发送初始时间
	if(i==0 && this_node = src_node){
	  start_time = time;
	  ++i;
	}

      #节点本身是sink_node，而数据包来源是src_node
      if(this_node == sink_node && pkt_src_id == src_node){
	#统计节点的累计接收数据包
	if(pkt_byte_sum[i] <= 0){
	  #printf("%s %.9f %d %d %d %d %d\n",event,time,this_node,pkt_src_id,pkt_dest_id,pkt_type,pkt_num);
	  pkt_byte_sum[i] = pkt_byte_sum[i-1] + pkt_size;
	  end_time[i] = time;
	}
	i = i+1;
      }
    }
  }
}
END{
  count = 0;
  average = 0.0;
  printf("%d\n",i);
  for(j=0;j < i;++j){
    duration = end_time[j] - start_time;
    #printf("bytes_recvd:%d,start_time:%f,end_time:%f\n",pkt_byte_sum[j],start_time,end_time[j]);
    if(duration > 0){
      th = pkt_byte_sum[j]/duration;
      average += th;
      count++;
      #printf("pkt_num:%d,start_time:%f,time:%f,throughput:%f Bps\n",j,start_time,end_time[j],th);
    }
  }
  printf("aloha:%f Bps\n",average/count);
}

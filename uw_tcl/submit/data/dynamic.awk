BEGIN {
    i = 0;
    i_time = 10;
}
{
    action = $1;
    time = $2;
    node_id = $3;
    pkt_position = $4;
    pkt_type = $7;
    pkt_size = $8;
    if(time<=i_time) {
        if(action == "r" && node_id == "_49_" && pkt_position == "AGT") {
            pkt_byte_sum[i] = pkt_byte_sum[i] + pkt_size;
        }
    }
    else {
        i_time = i_time + 10;
        i++;
    }
}

END{
    printf("%.2f \t %.2f\n", 0, 0);
    sum = 0;
    for(j=0 ; j<i+1 ; j++){
        th = pkt_byte_sum[j]/10*8/1000000;
        printf("%.4f \t %.4f\n", sum, th);
        sum += th;                                
    }
    for(k=0;k<i+1;k++){
        ave = ave + pkt_byte_sum[k]/10*8/1000;
    } 
    printf("the total throughput is %.4f,%d",ave/(i+1),i);    
}

#include<iostream>
#include "chanl_status_table.h"
#include<map>
#include<assert.h>
#include "uwaloha.h"
using namespace std;

int neighbor_nodes[] = {2,3,4};
extern const int MAXIMUMCOUNTER;
const double STATISTIC_TIMEOUT = 10.0;
//重载结构体chanl_status的输出操作符
ostream& operator<<(ostream& out,const chanl_status& sts){
    out << "target_node: " << sts.target_id
        << " pkt_id: " << sts.pkt_id
        << " is_acked: " << sts.is_acked << endl
        << " ack_time: " << sts.ack_time
        << " send_time: " << sts.send_time
        << " recv_time: " << sts.recv_time
        << " retry_times: " << sts.retry_times;
    return out;
}

Chanl_Status_Table::Chanl_Status_Table(int vector_capacity){
    table.reserve(vector_capacity);
}

void Chanl_Status_Table::put_into_table(const chanl_status &elem){
  /*检查该包是否已经在chan_status_table中了*/
  chanl_status* sts = find_elem(elem);
  if(sts->target_id > 0){
    sts->retry_times++;
  }else{
    table.push_back(elem);
  }
}

void Chanl_Status_Table::print_all_elements(){
    for(vector<chanl_status>::iterator it = table.begin();it != table.end();++it){
        cout << *it << endl;
    }
}

/*传进来一个chanl_status引用，根据该引用的target_id和pkt_id来查找元素
返回对vector中的元素的指针*/
chanl_status*
Chanl_Status_Table::find_elem(const chanl_status &elem){
    vector<chanl_status> &v = table;

    for(unsigned i=0; i < v.size(); i++){
        if(v[i].target_id == elem.target_id
            && v[i].pkt_id == elem.pkt_id)
            return (chanl_status*)&(v.at(i));
    }
    return &empty;
}

/*删除与elem具有相同pkt_id和target_id的所有元素*/
void Chanl_Status_Table::delete_elems(const chanl_status &elem){
    vector<chanl_status> &v = table;
    cout << "vector before delete: " << v.size() << endl;
    for(unsigned i=0; i < v.size() ; i++){
        if(v[i].target_id == elem.target_id
            && v[i].pkt_id == elem.pkt_id){
            v.erase(v.begin()+i);
            cout << "after delete: "<< v.size() << endl;
        }
    }
}

/*当接收到一个ACK数据包时，更新该数据结构*/
chanl_status*
Chanl_Status_Table::ack_elem(const chanl_status &elem){
    chanl_status* sts = find_elem(elem);
    if(sts->pkt_id > 0){
        sts->is_acked = elem.is_acked;
        sts->ack_time = elem.ack_time;
    }
    return sts;
}

/*当要重新发送数据包时，更新该target_id与pkt_id对应的统计数据*/
chanl_status*
Chanl_Status_Table::retry_elem(const chanl_status &elem){
    chanl_status* sts = find_elem(elem);
    if(sts->pkt_id > 0){
        //更新发送时间为：最近一次重发的时间
        sts->send_time = elem.send_time;
        sts->retry_times ++;
    }
    return sts;
}

double Chanl_Status_Table::evaluate_chanl(){
  vector<chanl_status>::iterator it;
  map<int,chanl_statistic> statistic;
  map<int,chanl_statistic>::iterator sit;
  for(it = table.begin();it != table.end();++it){
    int nexthop_id = it->target_id;
    //只统计最近时间的
    double statistic_time = NOW - it->send_time;
    if(statistic_time > STATISTIC_TIMEOUT)
    {
      printf("statistic_timeout:%f\n",statistic_time);
      continue;
    }
    sit = statistic.find(nexthop_id);
    if(sit != statistic.end()){
      sit->second.retry_times += it->retry_times;
      sit->second.success |= (it->is_acked > 0);
    }else{
      chanl_statistic stat(it->target_id,it->retry_times,(it->is_acked)>0);
      statistic.insert(pair<int,chanl_statistic>(it->target_id,stat));
    }
  }
  
  double factor = 0.0;
  /*输出map内所有的元素*/
  for(sit = statistic.begin();sit != statistic.end();++sit)
  {
    if(sit->second.success == false)
    {
      factor += 1.0;
    }
    else if(sit->second.retry_times > 0)
    {
      assert(MAXIMUMCOUNTER > 0);
      factor += (1.0*sit->second.retry_times) / MAXIMUMCOUNTER;
    }
//     cout << "nexthop_id:" << sit->second.nexthop_id 
//     << " retry_times:" << sit->second.retry_times
//     << " is successed:" << sit->second.success << endl;    
  }
  /*先返回一个临时值*/
/*  cout << factor << endl;*/
  return factor;
}

time_t get_curr_time(){
    time_t curr_time;
    struct tm *time_info;
    time(&curr_time);
    return curr_time;
}

/*测试：输出重载、打印所有元素、查找元素、添加元素、删除所有元素*/
void test_add_delete(){
    Chanl_Status_Table tb(20);
    chanl_status elem(5,5);
    chanl_status* res;
    cout << elem << endl;
    tb.print_all_elements();
    res = tb.find_elem(elem);
    if(res->pkt_id > 0)
        tb.put_into_table(elem);

    tb.delete_elems(elem);
    tb.print_all_elements();
}

/*测试：ACK元素、获得当前时间*/
void test_ack_elem(){
    Chanl_Status_Table tb(20);
    chanl_status elem(0,0);
    chanl_status* res;

    time_t curr_time;
    struct tm* time_info;

    elem.target_id = 4;
    elem.pkt_id = 2;
    res = tb.find_elem(elem);
    cout << *res << endl;
    sleep(3);
    elem.is_acked = 1;
    time(&curr_time);
    elem.ack_time = curr_time;
    res = tb.ack_elem(elem);
    if(res->target_id > 0)
        cout << *res << endl;
}

/*测试：重新发送数据包是更新*/
void test_retry_elem(){
    Chanl_Status_Table tb(20);
    chanl_status elem(0,0);
    chanl_status* res;

    elem.target_id = 4;
    elem.pkt_id = 2;
    elem.is_acked = 0;
    res = tb.find_elem(elem);
    if(res->pkt_id > 0)
        cout << *res << endl;
    //重新发送数据的时间
    elem.send_time = get_curr_time();
    elem.retry_times = 2;
    res = tb.retry_elem(elem);
    if(res->pkt_id > 0)
        cout << *res << endl;
}

// int main(){
// //    test_add_delete();
// //    test_ack_elem();
//     test_retry_elem();
//     return 0;
// }


#include<iostream>
#include<sstream>
#include<string>
#include<vector>
using namespace std;


vector<string> &spilit(const string &s, char delim,vector<string> &elems){
	stringstream ss(s);
	string item;
	while(getline(ss,item,delim)){
		elems.push_back(item);
	}
	return elems;
}

vector<string> spilit(const string &s,char delim){
	vector<string> elems;
	return spilit(s,delim,elems);
}

void spilit_by_blank(){
    string s("0,440,0,0 2,360,0,15 3,360,0,-15");
    istringstream iss(s);
    do{
        string sub;
        iss >> sub;
        cout << "substring:" << sub << endl;
    }while(iss);
}

int main(){
	string s("0,440,0,0|2,360,0,15|3,360,0,-15");
	vector<string> res = spilit(s,'|');
	while(!res.empty()){
		string sub = res.back();
		res.pop_back();
		cout << sub << endl;
	}
	return 0;
}

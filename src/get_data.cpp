#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <stdlib.h>
#include "get_data.h"
using namespace std; 

void get_data(MType &freq_ave_map,ifstream &in_file,int proce_num)
{
	MType::iterator it;
	vector<long double> *temp;
	long double freq, ignore;
	char c, name_suf[MAX_BUF_SIZE];
	string name_pre;
	while(!in_file.eof())
	{
		in_file >> ignore >> c >> freq >> c >> ignore >> name_pre;
		in_file.getline(name_suf,MAX_BUF_SIZE);
		string name =  name_pre+string(name_suf);
		if((it=freq_ave_map.find(name))==freq_ave_map.end()) continue;
		temp = it->second;
		temp->push_back(freq/proce_num);	
	}
}

void save_data(MType &freq_ave_map)
{
	cout << "I'm in save data" << endl;
	vector<long double> * temp;
	ofstream out_file("freqdata.data");
	if(!out_file.is_open())
	{
		cerr << "can not open freqdata.data" << endl;
		exit(0);
	}
	for(MType::iterator it=freq_ave_map.begin(); it!=freq_ave_map.end(); it++)
	{
		temp = it->second;
		out_file << it->first;
		for(vector<long double>::iterator ite=temp->begin(); ite!=temp->end(); ite++)
			out_file << "\t" << *ite;		 
		out_file << endl;
	}
	out_file.close();	
}
int retrive_data(int argc, char *argv[],int base_file,MType &freq_ave_map) 
{
	long double freq;
	long double ignore;
	//MType freq_ave_map;
	vector<long double> *sta_temp = NULL;
	double percent;
	string name_pre;
	char c, name_suf[MAX_BUF_SIZE]; 
        int i,file_num[]={24,36,48,96,192,384,768}; 
	sprintf(name_suf,"collect_info%s.txt",argv[1]);
	ifstream in_file(name_suf,ios_base::in);
	if(!in_file.is_open())
	{
		cerr << "open file collect_info" << argv[1] << ".txt wrong" << endl;
		return -1;
	}
	for(i=0; i<=19; i++)
	{
		sta_temp = new vector<long double>;//delete is not written
		in_file >> ignore >> c >> freq >> c >> ignore >> name_pre;
		in_file.getline(name_suf,MAX_BUF_SIZE);
		sta_temp->push_back(freq/base_file);
		freq_ave_map.insert(pair<string,vector<long double>* >(name_pre+string(name_suf),sta_temp));	
	}
	in_file.close();	
	cout << "what's the fuck" << endl;
	for(i=6; i>=0; i--)
	{
		cout << i << endl;
		if(file_num[i]>=base_file) continue;
		sprintf(name_suf,"collect_info%d.txt",file_num[i]);
		in_file.open(name_suf,ios_base::in);
		if(!in_file.is_open())
		{
			cerr << "can't open file collect_info " << name_suf << endl;
			return -1;
		}
		get_data(freq_ave_map,in_file,file_num[i]);		
		in_file.close();	
	}
	save_data(freq_ave_map);
	return 0;
}

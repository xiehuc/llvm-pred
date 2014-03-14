#include <iostream>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include <regex>


#define MAX_BUF_SIZE 200
using namespace std;

struct DType
{
	string func_id;
	unsigned long long freqence;
};

bool compare(DType num1, DType num2)
{
	return num1.freqence > num2.freqence;
}

int read_filelist(char * dir_name,vector<string> &filelist)
{
	struct dirent * file_info = NULL;
	string cur_dir = dir_name;
        DIR * dir = NULL;
	string file_name;
	try
	{
        	const regex pattern("llvmprof.out.*",regex::icase);//i don't know how to use RE
		if(cur_dir.at(cur_dir.length()-1) != '/')
		{
			cur_dir += "/";
		}
		if((dir = opendir(cur_dir.c_str()))==NULL)
		{
			cout << "ERROR:Open " << cur_dir << " failed!" << endl;
			return -1;
		}
		while((file_info = readdir(dir)) != NULL)
		{
			file_name = file_info->d_name;
			if(regex_match(file_name,pattern))
			{
				filelist.push_back(cur_dir+file_name);
			}
		}
	}
	catch(regex_error &e)
	{
		cout << e.what() << "\ncode: " << e.code() << endl;
	}
	
	return 0;
}

unsigned long long  parse_buf(string & info, vector<DType> &freq_count)
{
	int i,pos;
	char c_temp;
	double percent;
	string name, str_temp;
	long double freq_temp,sum_freq_temp;
	unsigned long long freqence,sum_freq;
      
	vector<DType>::iterator itr,itr_end;
	stringstream line_info(info,ios_base::in);
        
	line_info >> i >> c_temp >> percent >> c_temp >> freq_temp >> c_temp 
		  >> sum_freq_temp >> name;
	freqence = freq_temp;
	sum_freq = sum_freq_temp;
	
	name = name+info.substr(line_info.tellg());
        
	itr = freq_count.begin();
	itr_end = freq_count.end();
	while(itr != itr_end)
	{
		if(name.compare(itr->func_id)==0)
		{
			itr->freqence = itr->freqence+freqence;
			return sum_freq;
		}
		itr++;
	}
	freq_count.push_back(DType{name,freqence});
	return sum_freq;
}
unsigned long long extract_info(fstream &infile,vector<DType> &freq_count)
{
	int count = 0;
	unsigned long long sum_per_file;
	char buf[MAX_BUF_SIZE];
 
	while(count<2)
	{
		infile.getline(buf,MAX_BUF_SIZE);
		if(buf[1]=='#') count++;
	}
	while(!infile.eof())
	{
		infile.getline(buf,MAX_BUF_SIZE);
		string temp_str(buf);
		if(temp_str.length()==0) continue;
		sum_per_file = parse_buf(temp_str,freq_count);
	}
	return sum_per_file;
}
int main(int argc,char *argv[])
{
	vector<string> filelist;
	vector<DType> freq_count;
        string file_path,out_file_name;
        fstream infile,outfile;
	stringstream ss;
	double out_num;
        unsigned long long sum_freq = 0;

	if(argc != 2)
	{
		cout << "Usage: command [directory-where-the-files-exist]" << endl
 		     << "Like: ./a.out /home/lgz/Document/" << endl;
		return -1;
	}
	if(access(argv[1],R_OK)!=0)
	{
		cout << "Sorry,you can not read the director,check to see if the"
                     << " file exists or you have the right to read it" << endl;
                return -1;
        }

        read_filelist(argv[1],filelist);
	ss << "collect" << filelist.size() << ".txt";
	ss >> out_file_name;
	outfile.open(out_file_name,fstream::out);
        if(!outfile)
        {
		cout << "Error:Can't open the file " << out_file_name << endl;
		return -1;
	}

        while(!filelist.empty())
	{
		file_path = filelist.back();
		filelist.pop_back();
		infile.open(file_path,fstream::in);
		if(!infile)
		{
			cout << "Error:Can't open the file " << file_path << endl;
			return -1;
		}
		sum_freq += extract_info(infile,freq_count);
		infile.close();
	}
	sort(freq_count.begin(),freq_count.end(),compare);
	for(vector<DType>::iterator it = freq_count.begin();it!=freq_count.end();it++)
	{
		outfile  << (it->freqence*1.0/sum_freq)*100.0 << "%   \t" << it->freqence 
                         << "/" << sum_freq << "\t\t" << it->func_id << endl;
	}
	return 0;
}

#include <iostream>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include <regex>

/**
 *以前使用llvmprof处理二进制文件后，得到的可读文件分为两部分，
 *上半部分是TOP 20的函数，下半部分是TOP 20的基本块。都是用##标志。
 *使用llvmpred处理二进制文件后，得到的可读文件只有一部分，TOP 20
 *的基本块，也是用##标志。
 *在验证本程序的时候，使用的是以前的文件，里面是两部分，所以PART_NUM
 *定义为2。如果是处理现在的文件，里面只有一部分，就要将PART_NUM定义为1，
 *不然会出现死循环。
 **/
#define PART_NUM 2
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
	string cur_dir = dir_name, pat_pre("llvmprof.out."),pat_suf(".output");
	DIR * dir = NULL;
	string file_name;

	if(cur_dir.at(cur_dir.length()-1) != '/')  
		cur_dir += "/";

	if((dir = opendir(cur_dir.c_str()))==NULL)
	{
		cerr << "ERROR:Open " << cur_dir << " failed!" << endl;
		return -1;
	}
	while((file_info = readdir(dir)) != NULL)
	{
		file_name = file_info->d_name;
		if(file_name.size() <= 20) continue;
		if((file_name.compare(0,13,pat_pre)==0) && (file_name.compare(file_name.size()-7,7,pat_suf)==0))
			filelist.push_back(cur_dir+file_name);
	}
	return 0;
}

unsigned long long  parse_buf(string & buf, vector<DType> &freq_count)
{
	string name;
	char name_[MAX_BUF_SIZE];
	long double freq_temp,sum_freq_temp;
	unsigned long long freqence,sum_freq;

	sscanf(buf.c_str(),"%*d. %*f%% %Lf/%Lf\t%[^\n]",&freq_temp,&sum_freq_temp,name_);
	freqence = freq_temp;
	sum_freq = sum_freq_temp;
	name = string(name_);

	for(vector<DType>::iterator I=freq_count.begin(),E=freq_count.end();I!=E;++I)
	{
		if(name == I->func_id)
		{
			I->freqence += freqence;
			return sum_freq;
		}
	}
	freq_count.push_back(DType{name,freqence});
	return sum_freq;
}

unsigned long long extract_info(fstream &infile,vector<DType> &freq_count)
{
	int count = 0;
	unsigned long long sum_per_file;
	string buf;

	while(count<PART_NUM)
	{
		getline(infile,buf);
		if(buf[1]=='#') count++;
	}
	while(!infile.eof())
	{
		getline(infile,buf);
		if(buf.size() == 0) continue;
		sum_per_file = parse_buf(buf,freq_count);
	}
	return sum_per_file;
}

int main(int argc,char *argv[])
{
	vector<string> filelist;
	vector<DType> freq_count;
	string file_path;
	fstream infile;
	unsigned long long sum_freq = 0;

	if(argc != 2)
	{
		cerr << "Usage: "<<argv[0]<<" [directory-where-the-files-exist]" << endl
			<< "Like: "<<argv[0]<<" /home/lgz/Document/" << endl;
		return -1;
	}
	if(access(argv[1],R_OK)!=0)
	{
		cerr << "Sorry,you can not read the director,check to see if the"
			<< " file exists or you have the right to read it" << endl;
		return -1;
	}

	read_filelist(argv[1],filelist);
	
	while(!filelist.empty())
	{
		file_path = filelist.back();
		filelist.pop_back();
		infile.open(file_path,fstream::in);
		if(!infile)
		{
			cerr << "Error:Can't open the file " << file_path << endl;
			return -1;
		}
		sum_freq += extract_info(infile,freq_count);
		infile.close();
	}
	sort(freq_count.begin(),freq_count.end(),compare);
	for(vector<DType>::iterator it = freq_count.begin();it!=freq_count.end();it++)
	{
		cout << (it->freqence*1.0/sum_freq)*100.0 << "%   \t" << it->freqence 
			<< "/" << sum_freq << "\t\t" << it->func_id << endl;
	}
	return 0;
}

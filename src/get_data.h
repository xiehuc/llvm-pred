#ifndef _GET_DATA_H
#define _GET_DATA_H
#include <iostream>
#include <map>
#include <vector>
#define MAX_BUF_SIZE 200
using namespace std;
typedef map<string,vector<long double>*> MType;
int retrive_data(int argc, char *argv[], int base_file, MType &freq_ave_map);

#endif

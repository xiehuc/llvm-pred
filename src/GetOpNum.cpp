#include <iostream>
#include <fstream>
#include <string>
#include <map>

#include "preheader.h"
#include <llvm/Support/DataTypes.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/ADT/StringRef.h>

using namespace std;
using namespace llvm;

string BlockName;
map<unsigned, int> InstCount;
multimap<string, string> FunctionOpInfo;
ifstream fin;
ofstream fout;

//get the function name from a line
string getFunctionName(const string &line)
{
	string::size_type start, end;
	start = line.find("MOD_");
	if(start != string::npos)
	{
		start = line.find("__");
		end = start;
		while(line[end] != '(')
			end++;
		return line.substr(start, end-start);
	}
	else
	{
		start = line.find("MAIN");
		if(start != string::npos)
		{
			end = start;
			while(line[end] != '(')
				end++;
			return line.substr(start, end-start);
		}
		else
		{
			start = line.find("main");
			end = start;
			while(line[end] != '(')
				end++;
			return line.substr(start, end-start);
		}
	}
}

//get the basic block name from a line
string getBlockName(const string &line)
{
	string::size_type start, end;
	start = line.find("<");
	if(start != string::npos)
		return line.substr(start);
	else
	{
		start = line.find("entry");
		if(start != string::npos)
			return line.substr(start);
		else
		{
			start = line.find("return");
			return line.substr(start);
		}
	}
}

//get the block name and the function name from the input file
void getFunAndBlock()
{
	string str;

	while(getline(fin, str))
	{
		FunctionOpInfo.insert(make_pair(getFunctionName(str), getBlockName(str)));
	}

	for(map<string, string>::const_iterator iter = FunctionOpInfo.begin(); iter != FunctionOpInfo.end(); iter++)
	{
		cout << "Get Function Name:" << iter->first << endl;
		cout << "Get Block Name:" << iter->second << endl;
	}
}

//get the number of each kind of instructions
void getOpNum(Module* mod)
{
	iplist<Function>* FunctionList;
	FunctionList = &(mod->getFunctionList());

	/*for(iplist<Function>::const_iterator fun = FunctionList->begin(); fun != FunctionList->end(); fun++)
	{
		cout << "Function name:" << (fun->getName()).str() << endl;
 		for(Function::const_iterator bb = fun->begin(); bb != fun->end(); bb++)
			cout << (bb->getName()).str() << endl;
	}*/

	if(!FunctionList->empty())
	{
		for(iplist<Function>::const_iterator fun = FunctionList->begin(); fun != FunctionList->end(); fun++)
		{	
			//cout << (fun->getName()).str() << endl;
			multimap<string, string>::size_type entries = FunctionOpInfo.count((fun->getName()).str());
			if(entries != 0)
			{
				multimap<string, string>::iterator iter = FunctionOpInfo.find((fun->getName()).str());	
				for(multimap<string, string>::size_type t = 0; t != entries; t++, iter++)
				{
					for(Function::const_iterator bb = fun->begin(); bb != fun->end(); bb++)
					{
						if(strcmp((iter->second).c_str(), ((bb->getName()).str()).c_str()) == 0)//find the basic block
						{
							fout << (fun->getName()).str() << ',' << (bb->getName()).str() << ':' << endl;
							//cout << (fun->getName()).str() << ',' << (bb->getName()).str() << ':' << endl;

							InstCount.clear();
							for(BasicBlock::const_iterator inst = bb->begin(); inst != bb->end(); inst++)
							{
								InstCount[inst->getOpcode()]++;
							}

							for(map<unsigned,int>::const_iterator map_it = InstCount.begin(); map_it != InstCount.end(); map_it++)
							{
								fout << Instruction::getOpcodeName(map_it->first) << ":" << map_it->second << endl;
					    
								//cout << Instruction::getOpcodeName(map_it->first) << ":" << map_it->second << endl;
							}

							break;
						}
					}	
				}
			}
		}
	}
}


int main(int argc, char** argv)
{
	if(argc != 4)
	{
		cout << "Usage: GetOpNum [source].bc [hotspot].txt outputfile";
	}

	cout << "Calculate the number of each kind of instruction, please wait...." << endl;

	string inputfile, outputfile;
	
	inputfile.assign(argv[2]);
	outputfile.assign(argv[3]);

	fin.open(inputfile.c_str());
	fout.open(outputfile.c_str());

	getFunAndBlock();
	cout << "Finish reading inputfile" << endl;
	
	LLVMContext cnt;
	SMDiagnostic err;
	Module* mod;
	mod = ParseIRFile(argv[1], err, cnt);
	getOpNum(mod);

	fin.close();
	fout.close();
	fin.clear();
	fout.clear();

	cout << "Done!" << endl;

	return 0;
}

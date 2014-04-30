#ifndef PRED_DEBUG_H_H
#define PRED_DEBUG_H_H
//**********************************************
//  Must Include this File After All #include
//**********************************************

#include "config.h"
#include <llvm/Support/raw_ostream.h>

#ifdef ENABLE_DEBUG
#define DEBUG(expr) expr
#undef NDEBUG
#else
#define DEBUG(expr) 
#undef NDEBUG
#define NDEBUG
#endif

#include <assert.h> // NDEBUG macro would affact this

//disable some output code 
//dont use comment because consider code may used in future
//but comment may be delete sometimes
#define DISABLE(expr) 

#define RET_ON_FAIL(cond) if(!(cond)){outs()<<"Failed at:"<<__LINE__<<"\n";return NULL;}
#define VERBOSE(expr,verb) (expr || (outs()<<"<<HERE>>:"<<*verb<<"\n",0))


#ifdef ENABLE_DEBUG
//used for return void
#define ASSERT(expr,value,desc) if(!(expr)){outs()<<"\t"<<desc<<"\n"<<"\tFailed at:"<<__LINE__<<"\n"<<"\t"<<value<<"\n";return ;}
//used for return null
#define ASSERET(expr,value,desc) if(!(expr)){outs()<<"\t"<<desc<<"\n"<<"\tFailed at:"<<__LINE__<<"\n"<<"\t"<<value<<"\n";return NULL;}
#else
#define ASSERT(expr,value,desc) assert((expr ||(outs()<<__LINE__<<":"<<value<<"\n",0)) && desc)
#define ASSERET(expr,value,desc) ASSERT(expr,value,desc)
#endif

#endif

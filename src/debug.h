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

#define VERBOSE(expr,verb) (expr || (outs()<<"<<HERE>>:"<<*verb<<"\n",0))

// a assert with output llvm values
#define Assert(expr, value) assert(  expr || (outs()<<"\n>>>"<<value<<"<<<\n",0) )

// a assert which always need check
#define runtime_assert(expr) if( !(expr) ){ \
   outs()<<"Assert Failed:"<<__FILE__<<":"<<__LINE__<<"\n"; \
   assert(0);\
   exit(-1);\
}

#define ret_on_failed(expr,msg,ret) if(!(expr)){outs()<<"Failed at "<<__LINE__<<":"<<msg<<"\n"; return ret;}
#define ret_null_fail(expr,msg) ret_on_failed(expr,msg,NULL);

// ==========================================
//                 Duplicated
// ==========================================

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

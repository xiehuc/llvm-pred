#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <iterator>

#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_multifit_nlin.h>

#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/ManagedStatic.h>
#include <ProfileInfoLoader.h>
#include <ProfileInfoWriter.h>

#include "debug.h"

#define FUNC_NUM 31
#define PARA_MAX 10
#define EPSILON DBL_EPSILON
#define ERR(i) sqrt(gsl_matrix_get(covar,i,i))

struct Input{
   size_t N;
   std::string File;
   bool output; // true --- write out predicted result, false --- read as input
};

namespace llvm{
   struct InputParser: public cl::basic_parser<Input> {
      bool parse(cl::Option& O, StringRef ArgName, const std::string& ArgValue, Input& Val) {
         Val.File.resize(ArgValue.size());
         if(sscanf(ArgValue.c_str(), "%lu:%s", &Val.N, &Val.File.front()) != 2)
            return O.error("unexpected format of argument, need 'N:File', example: 24:merged.out, +96:predicted.out");
         if(ArgValue.front()=='+')
            Val.output = true;
         return false;
      }
   };
   cl::opt<bool> Verbose("v", cl::desc("print verbose info to help debug"));
   cl::opt<std::string> BitcodeFile(cl::Positional, cl::desc("<bitcode>"));
   cl::list<Input, bool, InputParser> Inputs(cl::Positional, 
         cl::value_desc("list of input and output files, with format: \
            N:prof.out read prof.out as X axis is N \
            +N:prof.out write predicted prof.out as X axis is N"), 
         cl::desc("<inputs>"),
         cl::OneOrMore);
}

using namespace llvm;
using namespace std;

struct data
{
	size_t n;
	long double  *y;
	int id;
};

struct best_fit
{
	int id;
   std::vector<long double> best_par;
};

std::vector<unsigned> x_data;
const unsigned para_num[]={2,2,3,2,3,3,2,3,3,3,3,3,3,3,2,3,1,4,3,3,3,4,3,4,4,3,4,4,5,5,3};
inline double FIT(gsl_vector* v, unsigned id)
{
   double ret = gsl_vector_get(v, id);
   if(ret < DBL_EPSILON && ret > -DBL_EPSILON) return 0.0;
   return ret;
}


long double cal_Yi(int id,const long double x,long double *a)
{
	long double result;
	switch(id)
	{
		case 0:  result = a[0]*powl(x,a[1]);				                      break;
		case 1:  result = a[0]*powl(a[1],x);                                  break;
		case 2:  result = a[0]-a[1]*powl(a[2],x);                             break;
		case 3:  result = 1-1/powl((1+a[0]*x),a[1]);                          break;
		case 4:  result = a[0]+a[1]*x+a[2]*powl(x,2);                         break;
		case 5:  result = a[0]*expl(a[1]*x+a[2]);                             break;	
		case 6:  result = a[0]*(1-expl(-a[1]*x));                             break;
		case 7:  result = a[0]+a[1]*powl(x,a[2]);                             break;	
		case 8:  result = a[0]+a[1]*expl(-a[2]*x);                            break;
		case 9:  result = a[0]+a[1]*expl(-x/a[2]);                            break;
		case 10: result = powl(a[0],a[1]/(x+a[2]));                           break;
		case 11: result = a[0]*powl(x-a[1],a[2]);                             break;
		case 12: result = powl(a[0]+a[1]*x,-1/a[2]);                          break; 
		case 13: result = a[0]-a[1]*logl(x+a[2]);                             break;
		case 14: result = a[0]*logl(a[0]*x-a[1]);                             break;
		case 15: result = a[0]*(1-expl(-a[1]*(x-a[2])));                      break;
		case 16: result = -logl(a[0])*powl(a[0],x);                           break;
		case 17: result = (a[0]+a[1]*x)/(a[2]+a[3]*x);                        break;
		case 18: result = a[0]/(1+a[1]*expl(-a[2]*x));                        break;
		case 19: result = a[0]/(1+expl(-a[1]*(x-a[2])));                      break;
		case 20: result = expl(a[0]+a[1]*x+a[2]*powl(x,2));                   break;
		case 21: result = a[0]*expl(-x/a[1])+a[2]+a[3]*x;                     break;
		case 22: result = 1/(a[0]+a[1]*powl(x,a[2]));                         break;
		case 23: result = a[0]+a[1]*x+a[2]*powl(a[3],x);                      break;
		case 24: result = a[0]+a[1]*expl(-(x-a[2])/a[3]);                     break;
		case 25: result = a[0]/(a[0]+a[1]*x+a[2]*powl(x,2));                  break;
		case 26: result = a[0]*x/(a[1]+a[2]*x+a[3]*powl(x,2));                break;
		case 27: result = a[0]+a[1]*x+a[2]*powl(x,2)+a[3]*powl(x,3);          break;
		case 28: result = a[0]+a[1]*expl(-x/a[2])+a[3]*expl(-x/a[4]);         break;
		case 29: result = a[0]+a[1]*(1-expl(-x/a[2]))+a[3]*(1-expl(-x/a[4])); break;
		case 30: result = a[0]*a[1]*powl(x,1-a[2])/(1+a[1]*powl(x,1-a[2]));   break;
      default: errs() << "wrong function style\n";                          break;
	}	
	return result;
}
void print_func(int id, double* para)
{
   switch(id){
      case 0: printf("%lf*x^%lf",          para[0], para[1]); break;
      case 1: printf("%lf*%lf^x",          para[0], para[1]); break;
      case 2: printf("%lf-%lf*%lf^x",      para[0], para[1], para[2]); break;
      case 3: printf("1-1/(1+%lf*x)^%lf",  para[0], para[1]); break;
      case 4: printf("%lf+%lf*x + %lf*x^2", para[0], para[1], para[2]); break;
      case 5: printf("%lf*e^(%lf*x+%lf)",  para[0], para[1], para[2]); break;
      case 6: printf("%lf*(1-e^{-%lf*x})", para[0], para[1]); break;
      case 7: printf("%lf+%lf*x^%lf",      para[0], para[1], para[2]); break;
      case 8: printf("%lf+%lf*e^(-%lf*x)", para[0], para[1], para[2]); break;
      default: break;
   }
}

void cal_d(int id,const long double x,long double *a, gsl_matrix *J,int i)
{
	switch(id)
	{
	case 0:
		gsl_matrix_set(J,i,0,powl(x,a[1]));
		gsl_matrix_set(J,i,1,a[0]*powl(x,a[1])*logl(x));
		break;
	case 1:
		gsl_matrix_set(J,i,0,powl(a[1],x));
		gsl_matrix_set(J,i,1,a[0]*powl(a[1],-1+x)*x);
		break;
	case 2:
		gsl_matrix_set(J,i,0,1);
		gsl_matrix_set(J,i,1,-powl(a[2],x));
		gsl_matrix_set(J,i,2,-a[1]*powl(a[2],-1+x)*x);
		break;
	case 3:
		gsl_matrix_set(J,i,0,a[1]*x*powl(1+a[0]*x,-1-a[1]));
		gsl_matrix_set(J,i,1,powl(1+a[0]*x,-a[1])*logl(1+a[0]*x));
		break;
	case 4:
		gsl_matrix_set(J,i,0,1);
		gsl_matrix_set(J,i,1,x);
		gsl_matrix_set(J,i,2,powl(x,2));
		break;
	case 5:
		gsl_matrix_set(J,i,0,expl(a[2]+a[1]*x));
		gsl_matrix_set(J,i,1,a[0]*expl(a[2]+a[1]*x)*x);
		gsl_matrix_set(J,i,2,a[0]*expl(a[2]+a[1]*x));
		break;
	case 6:
		gsl_matrix_set(J,i,0,1-expl(-a[1]*x));
		gsl_matrix_set(J,i,1,a[0]*expl(-a[1]*x)*x);
		break;
	case 7:
		gsl_matrix_set(J,i,0,1);
		gsl_matrix_set(J,i,1,powl(x,a[2]));
		gsl_matrix_set(J,i,2,a[1]*powl(x,a[2])*logl(x));
		break;
	case 8:
		gsl_matrix_set(J,i,0,1);
		gsl_matrix_set(J,i,1,expl(-a[2]*x));
		gsl_matrix_set(J,i,2,-a[1]*expl(-a[2]*x)*x);
		break;
	case 9:
		gsl_matrix_set(J,i,0,1);
		gsl_matrix_set(J,i,1,expl(-x/a[2]));
		gsl_matrix_set(J,i,2,a[1]*expl(-x/a[2])*x/powl(a[2],2));
		break;
	case 10:
		gsl_matrix_set(J,i,0,powl(a[0],-1+a[1]/(a[2]+x))*a[1]/(a[2]+x));
		gsl_matrix_set(J,i,1,powl(a[0],a[1]/(a[2]+x))*logl(a[0])/(a[2]+x));
		gsl_matrix_set(J,i,2,-powl(a[0],a[1]/(a[2]+x))*a[1]*logl(a[0])/powl(a[2]+x,2));
		break;
	case 11:
		gsl_matrix_set(J,i,0,powl(-a[1]+x,a[2]));
		gsl_matrix_set(J,i,1,-a[0]*a[2]*powl(-a[1]+x,-1+a[2]));
		gsl_matrix_set(J,i,2,a[0]*powl(-a[1]+x,a[2])*logl(-a[1]+x));
		break;
	case 12:
		gsl_matrix_set(J,i,0,-powl(a[0]+a[1]*x,-1-1/a[2])/a[2]);
		gsl_matrix_set(J,i,1,-x*powl(a[0]+a[1]*x,-1-1/a[2])/a[2]);
		gsl_matrix_set(J,i,2,powl(a[0]+a[1]*x,-1/a[2])*logl(a[0]+a[1]*x)/powl(a[2],2));
		break;
	case 13:
		gsl_matrix_set(J,i,0,1);
		gsl_matrix_set(J,i,1,-logl(a[2]+x));
		gsl_matrix_set(J,i,2,-a[1]/(a[2]+x));
		break;
	case 14:
		gsl_matrix_set(J,i,0,a[0]*x/(-a[1]+a[0]*x)+logl(-a[1]+a[0]*x));
		gsl_matrix_set(J,i,1,-a[0]/(-a[1]+a[0]*x));
		break;
	case 15:
		gsl_matrix_set(J,i,0,1-expl(-a[1]*(-a[2]+x)));
		gsl_matrix_set(J,i,1,-a[0]*expl(-a[1]*(-a[2]+x))*(a[2]-x));
		gsl_matrix_set(J,i,2,-a[0]*a[1]*expl(-a[1]*(-a[2]+x)));	
		break;
	case 16:
		gsl_matrix_set(J,i,0,-powl(a[0],-1+x)-powl(a[0],-1+x)*x*logl(a[0]));
		break;
	case 17:
		gsl_matrix_set(J,i,0,1/(a[2]+a[3]*x));
		gsl_matrix_set(J,i,1,x/(a[2]+a[3]*x));
		gsl_matrix_set(J,i,2,-(a[0]+a[1]*x)/powl(a[2]+a[3]*x,2));
		gsl_matrix_set(J,i,3,-x*(a[0]+a[1]*x)/powl(a[2]+a[3]*x,2));
		break;
	case 18:
		gsl_matrix_set(J,i,0,1/(1+a[1]*expl(-a[2]*x)));
		gsl_matrix_set(J,i,1,-a[0]*expl(-a[2]*x)/powl(1+a[1]*expl(-a[2]*x),2));
		gsl_matrix_set(J,i,2,a[0]*a[1]*expl(-a[2]*x)*x/powl(1+a[1]*expl(-a[2]*x),2));
		break;
	case 19:
		gsl_matrix_set(J,i,0,1/(1+expl(-a[1]*(-a[2]+x))));
		gsl_matrix_set(J,i,1,-a[0]*expl(-a[1]*(-a[2]+x))*(a[2]-x)/powl(1+expl(-a[1]*(-a[2]+x)),2));
		gsl_matrix_set(J,i,2,-a[0]*a[1]*expl(-a[1]*(-a[2]+x))/powl(1+expl(-a[1]*(-a[2]+x)),2));
		break;
	case 20:
		gsl_matrix_set(J,i,0,expl(a[0]+a[1]*x+a[2]*powl(x,2)));
		gsl_matrix_set(J,i,1,expl(a[0]+a[1]*x+a[2]*powl(x,2))*x);
		gsl_matrix_set(J,i,2,expl(a[0]+a[1]*x+a[2]*powl(x,2))*powl(x,2));
		break;
	case 21:
		gsl_matrix_set(J,i,0,expl(-x/a[1]));
		gsl_matrix_set(J,i,1,a[0]*expl(-x/a[1])*x/powl(a[1],2));
		gsl_matrix_set(J,i,2,1);
		gsl_matrix_set(J,i,3,x);
		break;
	case 22:
		gsl_matrix_set(J,i,0,-1/powl(a[0]+a[1]*powl(x,a[2]),2));
		gsl_matrix_set(J,i,1,-powl(x,a[2])/powl(a[0]+a[1]*powl(x,a[2]),2));
		gsl_matrix_set(J,i,2,-a[1]*powl(x,a[2])*logl(x)/powl(a[0]+a[1]*powl(x,a[2]),2));
		break;
	case 23:
		gsl_matrix_set(J,i,0,1);
		gsl_matrix_set(J,i,1,x);
		gsl_matrix_set(J,i,2,powl(a[3],x));
		gsl_matrix_set(J,i,3,a[2]*powl(a[3],-1+x)*x);
		break;
	case 24:
		gsl_matrix_set(J,i,0,1);
		gsl_matrix_set(J,i,1,expl((a[2]-x)/a[3]));
		gsl_matrix_set(J,i,2,a[1]*expl((a[2]-x)/a[3])/a[3]);
		gsl_matrix_set(J,i,3,-a[1]*expl((a[2]-x)/a[3])*(a[2]-x)/powl(a[3],2));
		break;
	case 25:
		gsl_matrix_set(J,i,0,-a[0]/powl(a[0]+a[1]*x+a[2]*powl(x,2),2)+1/(a[0]+a[1]*x+a[2]*powl(x,2)));
		gsl_matrix_set(J,i,1,-a[0]*x/powl(a[0]+a[1]*x+a[2]*powl(x,2),2));
		gsl_matrix_set(J,i,2,-a[0]*powl(x,2)/powl(a[0]+a[1]*x+a[2]*powl(x,2),2));
		break;
	case 26:
		gsl_matrix_set(J,i,0,x/(a[1]+a[2]*x+a[3]*powl(x,2)));
		gsl_matrix_set(J,i,1,-a[0]*x/powl(a[1]+a[2]*x+a[3]*powl(x,2),2));
		gsl_matrix_set(J,i,2,-a[0]*powl(x,2)/powl(a[1]+a[2]*x+a[3]*powl(x,2),2));
		gsl_matrix_set(J,i,3,-a[0]*powl(x,3)/powl(a[1]+a[2]*x+a[3]*powl(x,2),2));
		break;
	case 27:
		gsl_matrix_set(J,i,0,1);
		gsl_matrix_set(J,i,1,x);
		gsl_matrix_set(J,i,2,powl(x,2));
		gsl_matrix_set(J,i,3,powl(x,3));
		break;
	case 28:
		gsl_matrix_set(J,i,0,1);
		gsl_matrix_set(J,i,1,expl(-x/a[2]));
		gsl_matrix_set(J,i,2,a[1]*expl(-x/a[2])*x/powl(a[2],2));
		gsl_matrix_set(J,i,3,expl(-x/a[4]));
		gsl_matrix_set(J,i,4,a[3]*expl(-x/a[4])*x/powl(a[4],2));
		break;
	case 29:
		gsl_matrix_set(J,i,0,1);
		gsl_matrix_set(J,i,1,1-expl(-x/a[2]));
		gsl_matrix_set(J,i,2,-a[1]*expl(-x/a[2])*x/powl(a[2],2));
		gsl_matrix_set(J,i,3,1-expl(-x/a[4]));
		gsl_matrix_set(J,i,4,-a[3]*expl(-x/a[4])*x/powl(a[4],2));
		break;
	case 30:
		gsl_matrix_set(J,i,0,a[1]*powl(x,1-a[2])/(1+a[1]*powl(x,1-a[2])));
		gsl_matrix_set(J,i,1,a[0]*powl(x,1-a[2])/powl(1+a[1]*powl(x,1-a[2]),2));
		gsl_matrix_set(J,i,2,-a[0]*a[1]*powl(x,1-a[2])*logl(x)/powl(1+a[1]*powl(x,1-a[2]),2));
		break;
	}
}
int expb_f(const gsl_vector *x,void *data,gsl_vector *f)
{
	size_t i, n = ((struct data *)data)->n;
	long double a[20], Yi, *y = ((struct data *)data)->y;
	int id = ((struct data*)data)->id; 
	for(i=0; i<para_num[id]; i++)
		a[i] = gsl_vector_get(x,i);

	for(i=0;i<n;i++)
	{
		Yi = cal_Yi(id,x_data[i],a);
		gsl_vector_set(f,i,Yi-y[i]);
	}
	return GSL_SUCCESS;
}

int expb_df(const gsl_vector *x, void *data, gsl_matrix *J)
{
	size_t n = ((struct data *)data)->n;
	int id = ((struct data *)data)->id;
	size_t i;
	long double a[20];
	for(i=0; i<para_num[id]; i++)
		a[i] = gsl_vector_get(x,i);

	for(i=0;i<n;i++)
		cal_d(id,x_data[i],a,J,i);
	return GSL_SUCCESS;
}

int expb_fdf(const gsl_vector *x,void *data,gsl_vector*f,gsl_matrix *J)
{
	expb_f(x,data,f);
	expb_df(x,data,J);
	return GSL_SUCCESS;
}

inline bool lessthan(long double a, long double b)
{
   return a - b < -EPSILON;
}

long double cal_squaresum(gsl_vector *dif, int n, int fordebug)
{
	int i;
	long double sum=0, v_i;
	for(i=0; i<n; i++) {
		v_i = gsl_vector_get(dif,i);
		sum += v_i*v_i;
	}
	return sqrtl(sum);
}	

int main(int argc, char *argv[])
{
   llvm::cl::ParseCommandLineOptions(argc, argv);
   std::vector<std::vector<unsigned> > freq_ave;
   std::vector<std::string> BlockNames;

   // read bitcode;
   LLVMContext &Context = getGlobalContext();
   llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
   std::string ErrorMessage;
   error_code ec;
   Module *M = 0;
   auto Buffer = MemoryBuffer::getFileOrSTDIN(BitcodeFile);
   if (!(ec = Buffer.getError())){
      auto R = parseBitcodeFile(&**Buffer, Context);
      if(R.getError()){
         M = NULL;
         ErrorMessage = R.getError().message();
      }else
         M = R.get();
   } else
      ErrorMessage = ec.message();
   if (M == 0) {
      errs() << argv[0] << ": " << BitcodeFile << ": "
         << ErrorMessage << "\n";
      return 1;
   }

   // get block names
   for(auto& Func: *M){
      for(auto& Block : Func){
         BlockNames.push_back((Func.getName()+"() - "+Block.getName()).str());
      }
   }

   for(auto input : Inputs){
      if(input.output) continue;
      ProfileInfoLoader PIL(argv[0], input.File);
      auto& Counts = PIL.getRawBlockCounts();
      AssertRuntime(Counts.size() != 0, "Block Counters shouldn't be empty");
      if(freq_ave.size() == 0){
         freq_ave.resize(Counts.size());
         for(auto& v : freq_ave) 
            v.reserve(Inputs.size());
      }else
         Assert(freq_ave.size() == Counts.size(), "Profile data length unmatch");

      for(unsigned i=0;i<Counts.size();++i){
         freq_ave[i].push_back(Counts[i]);
      }
      x_data.push_back(input.N);
   }
   AssertRuntime(BlockNames.size() == freq_ave.size(), 
         "Module length not match, are u use before instrumented module?")

   unsigned X_NUM = Inputs.size();

   int status;
	unsigned i, j, p, n, k=0, iter=0;
	double para_init[PARA_MAX] = {1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0};
	const gsl_multifit_fdfsolver_type *T;
	long double pre_best_squsum, cur_squsum,* y_data;
   y_data = new long double[X_NUM];
	struct data d;
   std::vector<best_fit> params(freq_ave.size());
	gsl_multifit_fdfsolver *s;
	gsl_matrix *covar = NULL;
	gsl_multifit_function_fdf f;
	gsl_vector * dif;

   k = 0;
	for(auto& freq_vec : freq_ave)
	{
		pre_best_squsum = 1.7e+308;
		n = freq_vec.size();
      std::copy_n(freq_vec.begin(), freq_vec.size(), y_data);
      if(Verbose){
         cerr<<"[block."<<BlockNames[k]<<"   data:    ";
         copy(freq_vec.begin(), freq_vec.end(), ostream_iterator<unsigned>(cerr,","));
         cerr<<"]"<<endl;
      }
		
		for(j=0; j<FUNC_NUM; j++)
		{
			p = para_num[j];
			if(n<p) continue;
			covar = gsl_matrix_alloc(p,p);
			gsl_vector_view x = gsl_vector_view_array(para_init,p);

			d.n = n;
			d.y = y_data;
			d.id = j;
			
			f.f = &expb_f;
			f.df = &expb_df;
			f.fdf = &expb_fdf;
			f.n = n;
			f.p = p;
			f.params = &d;

			T = gsl_multifit_fdfsolver_lmsder;
			s = gsl_multifit_fdfsolver_alloc(T,n,p);
			gsl_multifit_fdfsolver_set(s,&f,&x.vector);
			iter = 0; 

			do{
				iter++;
				status = gsl_multifit_fdfsolver_iterate(s);
				if(status) break;
				
				status = gsl_multifit_test_delta(s->dx,s->x,1e-4,1e-4);
			}while(status == GSL_CONTINUE && iter < 500);

			gsl_multifit_covar(s->J,0.0,covar);
         //gsl_vector_set(dif,i,gsl_vector_get(s->f,i)-y_data[i]);
         dif = s->f;//xiehuc: s->f = f(x) - y[i], where y[i] is real data, and f(x) is guess data.
			cur_squsum = cal_squaresum(dif, n, j);//gsl_blas_dnrm2(dif);
         if(Verbose) {
            cerr<<"[func."<<j<<"    ";
            print_func(j, s->x->data);
            cerr<<"    squsum:"<<cur_squsum<<"]"<<endl;
         }
			if(lessthan(cur_squsum,pre_best_squsum)) {
				params[k].id = j;
            params[k].best_par.clear();
            for(i=0; i<p; i++)
               params[k].best_par.push_back(FIT(s->x, i));
            pre_best_squsum = cur_squsum;
			}
			gsl_multifit_fdfsolver_free(s);
			gsl_matrix_free(covar);

         if(pre_best_squsum < EPSILON)
            // we have found best fit, no need calc rest function
            break;
		}
      cout<<"func:"<<params[k].id<<"    args:";
		for(i=0; i<para_num[params[k].id]; i++)
			cout<< params[k].best_par.at(i) <<' ';
      cout<<"    "<<BlockNames[k]<<"\n";
		k++;
	}

   for(auto& input : Inputs) {
      if(!input.output) continue;
      ProfileInfoWriter PIW(argv[0], input.File);
      double X = input.N;
      vector<unsigned> Blocks;
      Blocks.reserve(params.size());
      for(auto& fit : params){
         Blocks.push_back(cal_Yi(fit.id, X, fit.best_par.data()));
      }
      PIW.write(BlockInfo, Blocks);
   }
   delete[] y_data;

   return 0;
}


#include <stdlib.h>
#include <stdio.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_multifit_nlin.h>
#include "get_data.h"

#include <llvm/Support/CommandLine.h>
#include <ProfileInfoLoader.h>

#include "debug.h"

#define FUNC_NUM 31
#define PARA_MAX 10
#define FIT(i) gsl_vector_get(s->x,i)
#define ERR(i) sqrt(gsl_matrix_get(covar,i,i))

struct Input{
   size_t N;
   std::string File;
};

namespace llvm{
   struct InputParser: public cl::basic_parser<Input> {
      bool parse(cl::Option& O, StringRef ArgName, const std::string& ArgValue, Input& Val) {
         Val.File.resize(ArgValue.size());
         if(sscanf(ArgValue.c_str(), "%lu:%s", &Val.N, &Val.File.front()) != 2)
            return O.error("unexpected format of argument, need 'N:File', example: 24:llvmprof.merged.out");
         return false;
      }
   };
   cl::list<Input, bool, InputParser> Inputs(cl::Positional, cl::desc("1"), cl::OneOrMore);
}

using namespace llvm;

struct data
{
	size_t n;
	long double  *y;
	int id;
};

struct best_fit
{
	int id;
	vector<long double> best_par;
};

int base_index, x_start=0;
std::vector<unsigned> x_data;
const unsigned para_num[]={2,2,2,2,3,3,3,3,3,3,3,3,3,3,2,3,1,4,3,3,3,4,3,4,4,3,4,4,5,5,3};


long double cal_Yi(int id,const long double x,long double *a)
{
	long double result;
	switch(id)
	{
		case 0:  result = a[0]*powl(x,a[1]);				                      break;
		case 1:  result = a[0]*powl(a[1],x);                                  break;
		case 2:  result = a[0]*(1-expl(-a[1]*x));                             break;
		case 3:  result = 1-1/powl((1+a[0]*x),a[1]);                          break;
		case 4:  result = a[0]+a[1]*x+a[2]*powl(x,2);                         break;
		case 5:  result = a[0]*expl(a[1]*x+a[2]);                             break;	
		case 6:  result = a[0]-a[1]*powl(a[2],x);                             break;
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
		default: cerr << "wrong function style" << endl;                      break;
	}	
	return result;
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
		gsl_matrix_set(J,i,0,1-expl(-a[1]*x));
		gsl_matrix_set(J,i,1,a[0]*expl(-a[1]*x)*x);
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
		gsl_matrix_set(J,i,0,1);
		gsl_matrix_set(J,i,1,-powl(a[2],x));
		gsl_matrix_set(J,i,2,-a[1]*powl(a[2],-1+x)*x);
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
		Yi = cal_Yi(id,x_data[i+x_start],a);
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
		cal_d(id,x_data[i+x_start],a,J,i);
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
   return a - b < -1e-5;
}
long double cal_squaresum(gsl_vector *dif,int n, int fordebug);

int main(int argc, char *argv[])
{
   llvm::cl::ParseCommandLineOptions(argc, argv);
   std::vector<std::vector<unsigned> > freq_ave;

   for(auto input : Inputs){
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

   unsigned X_NUM = Inputs.size();
   errs()<<freq_ave.size()<<"\n";

   int status;
	unsigned i, j, p, n, k=0, iter=0;
	double para_init[PARA_MAX] = {1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0};
	MType freq_ave_map;
	const gsl_multifit_fdfsolver_type *T;
	long double pre_best_squsum, cur_squsum,* y_data;
   y_data = new long double[X_NUM];
	struct data d;
	best_fit* params = new best_fit[freq_ave.size()];
	gsl_multifit_fdfsolver *s;
	gsl_matrix *covar = NULL;
	gsl_multifit_function_fdf f;
	gsl_vector * dif = gsl_vector_alloc(X_NUM);

   k = 0;
	for(auto& freq_vec : freq_ave)
	{
		pre_best_squsum = 1.7e+308;
		n = freq_vec.size();
		x_start = base_index-n+1;
      std::copy_n(freq_vec.begin(), freq_vec.size(), y_data);
		
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
			do
			{
				iter++;
				status = gsl_multifit_fdfsolver_iterate(s);
				if(status) break;
				
				status = gsl_multifit_test_delta(s->dx,s->x,1e-4,1e-4);
			}
			while(status == GSL_CONTINUE && iter < 500);
			gsl_multifit_covar(s->J,0.0,covar);
			for(i=0; i<n; i++) gsl_vector_set(dif,i,gsl_vector_get(s->f,i)-y_data[i]);
			cur_squsum = cal_squaresum(dif,n,j);//gsl_blas_dnrm2(dif);
			if(lessthan(cur_squsum,pre_best_squsum))
			{
				params[k].id = j;
				params[k].best_par.clear();
				for(i=0; i<p; i++)
					params[k].best_par.push_back(FIT(i));
				 pre_best_squsum = cur_squsum;
			}			
			gsl_multifit_fdfsolver_free(s);
			gsl_matrix_free(covar);
		}
      cerr<<"block."<<k<<"    func:"<<params[k].id<<"    args: ";
		for(i=0; i<para_num[params[k].id]; i++)
			cerr << params[k].best_par.at(i)<<' ';
      cerr<<std::endl;
		DISABLE(errs()<< "the least square is #" << pre_best_squsum << endl);
		k++;
	}
	gsl_vector_free(dif);
   delete[] y_data;
   delete[] params;
	return 0;
}

#if 0
bool lessthan(long double a, long double b)
{
	char science_a[20],science_b[20],char_a[8],char_b[8];
	float frac_a,frac_b;
	int exp_a,exp_b,i;

	if(!isfinite(a)) return false; 	
	sprintf(science_a,"%.5Le",a);
	sprintf(science_b,"%.5Le",b);
	sscanf(science_a+8,"%d",&exp_a);
	sscanf(science_b+8,"%d",&exp_b);

	if(exp_a < exp_b) return true;
	else if(exp_a > exp_b) return false;

	for(i=0; i<7; i++)
	{
		char_a[i] = science_a[i];
		char_b[i] = science_b[i];
	}
	char_a[i] = '\0';
	char_b[i] = '\0';
	frac_a = atof(char_a);
	frac_b = atof(char_b);
	if(frac_a < frac_b) return true;
	return false;
}
#endif

long double cal_squaresum(gsl_vector *dif, int n, int fordebug)
{
	int i;
	long double num=0, temp;
	for(i=0; i<n; i++)
	{
		temp = gsl_vector_get(dif,i);
		num = num+temp*temp;
	}
	return sqrtl(num);
}	

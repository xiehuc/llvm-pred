#include <stdlib.h>
#include <stdio.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_multifit_nlin.h>

struct data
{
	size_t n;
	double  *y;
	double *sigma;
};

int x_data[6]={24,36,48,96,192,384};
int expb_f(const gsl_vector *x,void *data,gsl_vector *f)
{
	size_t n = ((struct data *)data)->n;
	double *y = ((struct data *)data)->y;
	double *sigma = ((struct data *)data)->sigma;
	double a = gsl_vector_get(x,0);
	double b = gsl_vector_get(x,1);
	
	size_t i;
	for(i=0;i<n;i++)
	{
		double t = x_data[i];
		double Yi = a*pow(t,b);
		gsl_vector_set(f,i,(Yi-y[i])/sigma[i]);
	}
	return GSL_SUCCESS;
}

int expb_df(const gsl_vector *x, void *data, gsl_matrix *J)
{
	size_t n = ((struct data *)data)->n;
	double *sigma = ((struct data *)data)->sigma;
	double a = gsl_vector_get(x,0);
	double b = gsl_vector_get(x,1);
	size_t i;
	for(i=0;i<n;i++)
	{
		gsl_matrix_set(J,i,0,pow(x_data[i],b)/sigma[i]);
		gsl_matrix_set(J,i,1,a*pow(x_data[i],b)*logl(x_data[i])/sigma[i]);
	}
	return GSL_SUCCESS;
}

int expb_fdf(const gsl_vector *x,void *data,gsl_vector*f,gsl_matrix *J)
{
	expb_f(x,data,f);
	expb_df(x,data,J);
	return GSL_SUCCESS;
}

void print_state(size_t iter,gsl_multifit_fdfsolver *s);

int main(void)
{

	double y_observe[6]={244570189.5,163046793,122285094.8,61142547.38,30571273.69,15285622.41};
	double sigma[6]={1,1,1,1,1,1};
	const gsl_multifit_fdfsolver_type *T;
	gsl_multifit_fdfsolver *s;
	int status;
	unsigned int i, iter = 0;
	const size_t n = 6;
	const size_t p = 2;
	gsl_matrix *covar = gsl_matrix_alloc(p,p);
	struct data d = {n,y_observe,sigma};
	gsl_multifit_function_fdf f;
 	double para_init[2] = {1.0,1.0};
	gsl_vector_view x = gsl_vector_view_array(para_init,p);
	
	f.f = &expb_f;
	f.df = &expb_df;
	f.fdf = &expb_fdf;
	f.n = n;
	f.p = p;
	f.params = &d;

	T = gsl_multifit_fdfsolver_lmsder;
	s = gsl_multifit_fdfsolver_alloc(T,n,p);
	gsl_multifit_fdfsolver_set(s,&f,&x.vector);
	print_state(iter,s);
	do
	{
		iter++;
		status = gsl_multifit_fdfsolver_iterate(s);
		printf("status = %s\n",gsl_strerror(status));
		print_state(iter,s);
		if(status) break;
		
		status = gsl_multifit_test_delta(s->dx,s->x,1e-4,1e-4);
	}
	while(status == GSL_CONTINUE && iter < 500);
	gsl_multifit_covar(s->J,0.0,covar);
#define FIT(i) gsl_vector_get(s->x,i)
#define ERR(i) sqrt(gsl_matrix_get(covar,i,i))

	{
		double chi = gsl_blas_dnrm2(s->f);
		double dof = n-p;
		double c = GSL_MAX_DBL(1,chi/sqrt(dof));
		printf("chisq/dof = %g\n",pow(chi,2.0)/dof);
		printf("a	=%.5f +/- %.5f\n",FIT(0),c*ERR(0));
		printf("b	=%.5f +/- %.5f\n",FIT(1),c*ERR(1));
	}
	printf("status = %s\n",gsl_strerror(status));
	gsl_multifit_fdfsolver_free(s);
	gsl_matrix_free(covar);
	return 0;
}
void print_state(size_t iter,gsl_multifit_fdfsolver *s)
{
	printf("iter: %3u x = %15.8f %15.8f  |f(x)| = %g\n",iter,gsl_vector_get(s->x,0),
		gsl_vector_get(s->x,1),gsl_blas_dnrm2(s->f));
}

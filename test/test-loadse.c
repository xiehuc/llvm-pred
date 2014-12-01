#include "../src/libtiming.c"

#define REPEAT 1000
/* use a template to generate instruction */
int inst_template(const char* templ, ...);
int main()
{
   volatile int* loadvars = malloc(10000*sizeof(int));
   volatile int* loadvare = loadvars+9999;
   //uint64_t t_res = timing_res();
   uint64_t t_err = timing_err();
   unsigned long beg = 0, end = 0, sum = 0, ref = 0;
   for(int i=0;i<REPEAT;i++){
      beg = timing();
      ref = inst_template("loadse",loadvars,loadvare);
      end = timing();
      sum += end-beg-t_err;
   }
   sum /= REPEAT;
   printf("loadse Inst Time:%lu, ref:20000,no use:%lu\n", sum,ref);
}

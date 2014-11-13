#include "../src/libtiming.c"

#define REPEAT 100
/* use a template to generate instruction */
int inst_template(const char* templ, ...);
int main()
{
   volatile int* loadvar = malloc(sizeof(int));
   uint64_t t_res = timing_res();
   uint64_t t_err = timing_err();
   unsigned long beg = 0, end = 0, sum = 0, ref = 0;
   for(int i=0; i<REPEAT; ++i){
      beg = timing();
      //ref += load_template("load");
      inst_template("load",loadvar);
      end = timing();
      sum += end-beg-t_err;
   }
   sum /= REPEAT;
   ref /= REPEAT;
   printf("ref:%lu\n", ref);
   printf("%lu\n", sum);
}

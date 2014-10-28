#include "../src/libtiming.c"

#define REPEAT 10
/* use a template to generate instruction */
int inst_template(const char* templ);

int main()
{
   uint64_t t_res = timing_res();
   uint64_t t_err = timing_err();
   //uint64_t beg, end, sum = 0, ref = 0;
   unsigned long beg = 0, end = 0, sum = 0, ref = 0;
   for(int i=0; i<REPEAT; ++i){
      beg = timing();
      printf("beg is %lu\n",beg); 
      ref += inst_template("fix_add");
      end = timing();
      //printf("end=%lu, \n",end);
      sum += end-beg-t_err;
      //printf("sum=%lu\n",sum);
   }
   //sum /= 10.0;
   //printf("ref:%lu\n", ref);
   sum /= REPEAT;
   printf("ref:%lu\n", ref);
   printf("%lu\n", sum);
}

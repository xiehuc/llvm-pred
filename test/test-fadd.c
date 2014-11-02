#include "../src/libtiming.c"

#define REPEAT 100
/* use a template to generate instruction */
int inst_template(const char* templ);

int main()
{
   int i, ref = 0;
   uint64_t beg, end, sum = 0;
   uint64_t t_err = timing_err();
   for(i=0;i<REPEAT;++i){
      beg = timing();
      ref += inst_template("float_add");
      end = timing();
      sum += end-beg-t_err;
   }
   sum /= REPEAT;
   ref /= REPEAT;
   printf("float inst T:%lu, ref:%d\n", sum, ref);
   return 0;
}


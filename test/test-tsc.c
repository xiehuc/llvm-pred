#include "../src/libtiming.c"

#define R5(x) x; x; x; x; x;
#define R25(x) R5(R5(x));
/* Repeat Hundred */
#define RH(x) R25(x); R25(x); R25(x); R25(x);
/* Repeat Thousand */
#define RT(x) R5(RH(x)); R5(RH(x));
#define REPEAT 10
int inst_template();

int main()
{
   uint64_t t_res = timing_res();
   uint64_t t_err = timing_err();
   uint64_t beg, end, sum = 0, ref = 0;
   for(int i=0; i<REPEAT; ++i){
      beg = timing();
      ref += inst_template();
      end = timing();
      sum += end-beg-t_err;
   }
   sum /= 10;
   printf("ref:%lu\n", ref);
   printf("%lu\n", sum);
}

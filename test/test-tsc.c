#include "../src/libtiming.c"

#define R5(x) x; x; x; x; x;
#define R25(x) R5(R5(x));
/* Repeat Hundred */
#define RH(x) R25(x); R25(x); R25(x); R25(x);
/* Repeat Thousand */
#define RT(x) R5(RH(x)); R5(RH(x));

int main()
{
   uint64_t t_res = timing_res();
   uint64_t t_err = timing_err();
   int c = 1;
   uint64_t beg = timing();
   RT(c = c+1);
   uint64_t end = timing();
   printf("%lu\n", end-beg-t_err);
}

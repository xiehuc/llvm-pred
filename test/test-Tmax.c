#include "../src/libtiming.c"
#define REPEAT 100

/** this is a test for valid which fomular is better:
 * 1. T = max(T_float, T_fix)
 * 2. T = T_float + T_fix
 */
int inst_template(const char*);

void test_order()
{
   int i, ref = 0;
   uint64_t beg, end, sum = 0;
   uint64_t t_err = timing_err();
   for(i=0;i<REPEAT;++i){
      beg = timing();
      ref += inst_template("fix_add");
      ref += inst_template("float_add");
      end = timing();
      sum += end-beg-t_err;
   }
   sum /= REPEAT;
   printf("order inst T:%lu, ref:%d\n", sum, ref);
}

void test_mix()
{
   int i, ref = 0;
   uint64_t beg, end, sum = 0;
   uint64_t t_err = timing_err();
   for(i=0;i<REPEAT;++i){
      beg = timing();
      ref += inst_template("mix_add");
      end = timing();
      sum += end-beg-t_err;
   }
   sum /= REPEAT;
   printf("mix inst T:%lu, ref:%d\n", sum, ref);
}

int main()
{
   test_order();
   test_mix();
}

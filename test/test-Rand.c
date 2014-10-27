#include "../src/libtiming.c"
#define REPEAT 100

int inst_template(const char*);

int main()
{
   int i, ref = 0;
   uint64_t beg, end, sum=0;
   uint64_t t_err = timing_err();
   for(i=0;i<REPEAT;++i){
      beg = timing();
      ref += inst_template("rand_add");
      end = timing();
      sum += end - beg - t_err;
   }
   sum /= REPEAT;
   printf("mix inst T:%lu, ref:%d\n", sum, ref);
}

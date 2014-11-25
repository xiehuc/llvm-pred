#include <stdlib.h>
#ifdef TIMING_SOURCE_lmbench
void load_timing_source(int* cpu_times)
{
   char* file = getenv("TIMING_SOURCE");
   if(file == NULL){
      fprintf(stderr,"no environment variable TIMING_SOURCE");
      exit(-1);
   }
   FILE* f = fopen(file,"r");
   if(f == NULL) perror("Could not open file:");

   double nanosec;
   char bits[48], ops[48];
   while(fscanf(f, "%s %s: %lf", bits, ops, &nanosec)){
      unsigned bit,op;
      bit = (bits=="integer")?Integer:(bits=="int64")?I64:(bits=="float")?Float:(bits=="double")?Double:-1U;
      op = (ops=="add")?Add:(ops=="mul")?Mul:(ops=="div")?Div:(ops=="mod")?Mod:-1U;
      if(bit == -1U || op == -1U) continue;
      cpu_times[bit|op] = nanosec;
   }
}
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "libtiming.h"
#include "config.h"
#ifdef TIMING_SOURCE_lmbench
#define eq(a,b) (strcmp(a,b)==0)
void load_timing_source(int* cpu_times)
{
   char* file = getenv("TIMING_SOURCE");
   if(file == NULL) file = "lmbench.log";
   FILE* f = fopen(file,"r");
   if(f == NULL){
      fprintf(stderr, "Could not open %s file: %s", file, strerror(errno));
      exit(-1);
   }

   double nanosec;
   char bits[48], ops[48];
   char line[512];
   while(fgets(line,sizeof(line),f)){
      unsigned bit,op;
      if(sscanf(line, "%s %[^:]: %lf", bits, ops, &nanosec)<3) continue;
      bit = eq(bits,"integer")?Integer:eq(bits,"int64")?I64:eq(bits,"float")?Float:eq(bits,"double")?Double:-1U;
      op = eq(ops,"add")?Add:eq(ops,"mul")?Mul:eq(ops,"div")?Div:eq(ops,"mod")?Mod:-1U;
      if(bit == -1U || op == -1U) continue;
      cpu_times[bit|op] = nanosec*100;
   }
#ifndef NO_DEBUG
   unsigned i;
   printf("cpuTimes: ");
   for(i=0;i<NumGroups;++i){
      printf("%d,", cpu_times[i]);
   }
   printf("\n");
#endif
}
#endif

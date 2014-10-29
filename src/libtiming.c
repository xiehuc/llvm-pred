#include "config.h"

/**
 * function: timing_res 
 * return once timing's resolution, nanosecond unit
 *
 * function: timing_err
 * return an error between two timing's, should sub this value
 *
 * function: timing
 * return a timing, mul timing_res to calc real time
 */

#ifdef TIMING_TSC
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

/* We use 64bit values for the times.  */
typedef unsigned long long int hp_timing_t;

/* The "=A" constraint used in 32-bit mode does not work in 64-bit mode.  */
#define HP_TIMING_NOW(Var) \
  ({ unsigned int _hi, _lo; \
     asm volatile ("rdtsc" : "=a" (_lo), "=d" (_hi)); \
     (Var) = ((unsigned long long int) _hi << 32) | _lo; })

uint64_t timing_res() 
{
   const char* file = "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq";
   FILE* f = fopen(file,"r");
   if(f==NULL){
      perror("Unable Find CPU freq sys file:");
      exit(errno);
   }
   unsigned long freq = 0;
   fscanf(f, "%lu", &freq);
   if(freq==0){
      perror("Unable Read CPU freq:");
      exit(errno);
   }
   return 1*1000*1000*1000 /*nanosecond*/ / freq;
}
uint64_t timing_err()
{
   hp_timing_t a = 0,b = 0;
   HP_TIMING_NOW(a);
   HP_TIMING_NOW(b);
   return b-a;
}
uint64_t timing()
{
   hp_timing_t t;
   HP_TIMING_NOW(t);
   return t;
}
#endif

#ifdef TIMING_CLOCK_GETTIME
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define CLK_ID CLOCK_PROCESS_CPUTIME_ID

uint64_t timing_res() 
{
   return 1;
}
uint64_t timing_err()
{
   struct timespec a = {0}, b = {0};
   long sum = 0;
   for(int i=0;i<100;++i){
      clock_gettime(CLK_ID, &a);
      clock_gettime(CLK_ID, &b);
      sum += (b.tv_sec - a.tv_sec)*10E9;
      sum += b.tv_nsec - a.tv_nsec;
   }
   return sum /=100;
}
uint64_t timing()
{
   struct timespec t = {0};
   clock_gettime(CLK_ID, &t);
   return t.tv_sec*10E9+t.tv_nsec;
}
#endif

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

#ifdef USING_TSC
static uint64_t timing_res() 
{
}
static uint64_t timing_err()
{
}
static uint64_t timing()
{
}
#endif

#ifdef USING_CLOCK_GETTIME
static uint64_t timing_res() 
{
}
static uint64_t timing_err()
{
}
static uint64_t timing()
{
}
#endif

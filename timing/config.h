#ifndef LLE_CONFIG_H_H
#define LLE_CONFIG_H_H

#define LLVM_VERSION_MAJOR 3
#define LLVM_VERSION_MINOR 5
#ifndef NO_DEBUG
/* #undef NO_DEBUG */
#endif
#define TIMING_tsc
#define TIMING_SOURCE_lmbench
#define PROMOTE_FREQ_path_prob

//hide clobber result when doing memory analysis dependencies
#define HIDE_CLOBBER 1
// DDGraph::expr() print ref number. to simplify presentation
#define EXPR_ENABLE_REF 
// a special symbol for self reference
#define PHINODE_CIRCLE "Δ"
// a ddg would produce better string expr for loop cycle(tripcount)cycle
#define CYCLE_EXPR_USE_DDG

#endif

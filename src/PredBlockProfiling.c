#include "Profiling.h"
#include <stdlib.h>

static unsigned *ArrayStart;
static unsigned NumElements;

static void PredBlockProfAtExitHandler(void) {
  write_profiling_data(BlockInfo, ArrayStart, NumElements);
}

int llvm_start_pred_block_profiling(int argc, const char **argv,
                              unsigned *arrayStart, unsigned numElements) {
  int Ret = save_arguments(argc, argv);
  ArrayStart = arrayStart;
  NumElements = numElements;
  atexit(PredBlockProfAtExitHandler);
  return Ret;
}

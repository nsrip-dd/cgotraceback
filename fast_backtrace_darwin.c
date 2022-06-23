#include <stddef.h>

// There isn't a way (that I've found) to make __attribute__ ((weak)), or
// something similar, work with the linker on macos. LLVM libunwind doesn't have
// a faster unwinding function anyway, so we leave it NULL for now.
int (*cgo_traceback_fast_backtrace)(void **stack, int max) = NULL;
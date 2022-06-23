// The "nongnu" libunwind implementation provides a fast unwinding function
// called unw_backtrace that bypasses the normal context/cursor setup. It's
// sufficiently fast that it's better to use unw_backtrace, even in cgo_context.
// LLVM's libunwind doesn't support it, so we weakly define it to let it be
// overridden if another implementation is available.
__attribute__ ((weak)) int unw_backtrace(void **stack, int max);
int (*cgo_traceback_fast_backtrace)(void **stack, int max) = unw_backtrace;
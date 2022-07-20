#ifndef CGO_TRACEBACK_H
#define CGO_TRACEBACK_H

#include <stdint.h>

struct cgo_symbolizer_args {
        uintptr_t pc;
        const char* file;
        uintptr_t lineno;
        const char* func;
        uintptr_t entry;
        uintptr_t more;
        uintptr_t data;
};

#endif
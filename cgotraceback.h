#ifndef CGO_TRACEBACK_H
#define CGO_TRACEBACK_H

#include <stdint.h>
#include <stdlib.h>

struct cgo_symbolizer_args {
        uintptr_t pc;
        const char* file;
        uintptr_t lineno;
        const char* func;
        uintptr_t entry;
        uintptr_t more;
        uintptr_t data;
};

int cgo_traceback_is_mangled(const char *func);
char *cgo_traceback_demangle(const char *mangled_name, char *buf, size_t *n, int *status);

#endif
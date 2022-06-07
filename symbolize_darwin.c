#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cgotraceback.h"

void cgo_symbolizer(void* p) {
        struct cgo_symbolizer_args *arg = p;

        if (arg->pc == 0) {
                free((void *) arg->data);
                return;
        }

        Dl_info dlinfo;
        if (dladdr((void *) arg->pc, &dlinfo) == 0) {
                arg->file = "?";
                arg->func = "?";
                arg->entry = 0;
                return;
        }
        arg->file = dlinfo.dli_fname;
        arg->func = dlinfo.dli_sname;
        if (cgo_traceback_is_mangled(arg->func)) {
                arg->func = cgo_traceback_demangle(arg->func, NULL, NULL, NULL);
                arg->data = (uintptr_t) arg->func;
        }
        arg->entry = (uintptr_t) dlinfo.dli_saddr;
}
#include <dlfcn.h>
#include <stdint.h>

void cgo_symbolizer(void* p) {
        struct {
                uintptr_t pc;
                const char* file;
                uintptr_t lineno;
                const char* func;
                uintptr_t entry;
                uintptr_t more;
                uintptr_t data;
        }* arg = p;

        Dl_info dlinfo;
        if (dladdr((void *) arg->pc, &dlinfo) == 0) {
                arg->file = "?";
                arg->func = "?";
                arg->entry = 0;
                return;
        }
        arg->file = dlinfo.dli_fname;
        arg->func = dlinfo.dli_sname;
        arg->entry = (uintptr_t) dlinfo.dli_saddr;
}
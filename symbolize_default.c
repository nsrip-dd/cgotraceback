//go:build darwin || !use_libdwfl
// +build darwin !use_libdwfl

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cgotraceback.h"

void cgo_symbolizer(void* p) {
        struct cgo_symbolizer_args *arg = p;

        if (arg->pc == 0) {
                return;
        }

        Dl_info dlinfo;
        if (dladdr((void *) arg->pc, &dlinfo) == 0) {
                return;
        }
        arg->file = dlinfo.dli_fname;
        arg->func = dlinfo.dli_sname;
        arg->entry = (uintptr_t) dlinfo.dli_saddr;
}
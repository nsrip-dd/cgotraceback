//go:build linux && use_libdwfl
// +build linux
// +build use_libdwfl

#define _GNU_SOURCE
#include <link.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <elfutils/libdwfl.h>
#include <pthread.h>

#include "cgotraceback.h"

static pthread_mutex_t dwfl_lock = PTHREAD_MUTEX_INITIALIZER;
static Dwfl *dwfl = NULL;

static const Dwfl_Callbacks dwfl_callbacks = {
        .find_elf = dwfl_linux_proc_find_elf,
        .find_debuginfo = dwfl_standard_find_debuginfo,
};

static int dl_callback(struct dl_phdr_info *info, size_t size, void *data);

__attribute__ ((constructor)) static void init(void) {
        dwfl = dwfl_begin(&dwfl_callbacks);
        if (dwfl == NULL) {
                return;
        }

        dwfl_report_begin(dwfl);
        int count = 0;
        dl_iterate_phdr(dl_callback, &count);
        dwfl_report_end(dwfl, NULL, NULL);
}

static int dl_callback(struct dl_phdr_info *info, size_t size, void *data) {
        int *count = data;
        const char *file = NULL;
        const char *name = NULL;
        if ((*count)++ == 0) {
                // The first thing we visit is the executable
                char filename[2048];
                int n = readlink("/proc/self/exe", filename, 2047);
                filename[n] = 0;
                file = filename;
                name = "self";
        } else {
                file = info->dlpi_name;
                name = info->dlpi_name;
        }
        dwfl_report_elf(
                dwfl,
                name,
                file,
                -1, // FD (-1 for none)
                info->dlpi_addr,
                0 // add p_vaddr
        );
        return 0;
}

void cgo_symbolizer(void *p) {
        pthread_mutex_lock(&dwfl_lock);
        if (dwfl == NULL) {
                pthread_mutex_unlock(&dwfl_lock);
                return;
        }

        struct cgo_symbolizer_args *args = p;
        if (args->pc == 0) {
                pthread_mutex_unlock(&dwfl_lock);
                return;
        }

        Dwfl_Module *module = dwfl_addrmodule(dwfl, args->pc);
        if (module == NULL) {
                pthread_mutex_unlock(&dwfl_lock);
                return;
        }

        const char *func = dwfl_module_addrname(module, args->pc);
        args->func = func;
        Dwfl_Line *line = dwfl_module_getsrc(module, args->pc);
        if (line == NULL) {
                pthread_mutex_unlock(&dwfl_lock);
                return;
        }
        int line_number = 0;
        args->file = dwfl_lineinfo(line, NULL, &line_number, NULL, NULL, NULL);
        args->lineno = (uintptr_t) line_number;
        pthread_mutex_unlock(&dwfl_lock);
}
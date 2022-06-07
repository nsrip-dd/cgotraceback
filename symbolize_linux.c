#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <elfutils/libdwfl.h>

#include "cgotraceback.h"

static Dwfl *dwfl;

static const Dwfl_Callbacks dwfl_callbacks = {
      .find_elf = dwfl_linux_proc_find_elf,
      .find_debuginfo = dwfl_standard_find_debuginfo,
};

__attribute__ ((constructor)) static void init(void) {
        dwfl = dwfl_begin(&dwfl_callbacks);
        if (dwfl == NULL) {
                return;
        }

        char filename[2048];
        int n = readlink("/proc/self/exe", filename, 2047);
        filename[n] = 0;
        dwfl_report_begin(dwfl);
        dwfl_report_elf(dwfl, "self", filename, -1, 0, 0);
        dwfl_report_end(dwfl, NULL, NULL);
}

void cgo_symbolizer(void *p) {
        struct cgo_symbolizer_args *args = p;
        if (args->pc == 0) {
                free((void *) args->data);
                return;
        }

        Dwfl_Module *module = dwfl_addrmodule(dwfl, args->pc);
        if (module == NULL) {
                return;
        }

        const char *func = dwfl_module_addrname(module, args->pc);
        if (cgo_traceback_is_mangled(func)) {
                func = cgo_traceback_demangle(func, NULL, NULL, NULL);
                args->data = (uintptr_t) func;
        }
        args->func = func;
        Dwfl_Line *line = dwfl_module_getsrc(module, args->pc);
        if (line == NULL) {
                return;
        }
        int line_number = 0;
        args->file = dwfl_lineinfo(line, NULL, &line_number, NULL, NULL, NULL);
        args->lineno = (uintptr_t) line_number;
}
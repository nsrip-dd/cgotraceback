#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <elfutils/libdwfl.h>

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

struct cgo_symbolizer_args {
        uintptr_t pc;
        const char* file;
        uintptr_t lineno;
        const char* func;
        uintptr_t entry;
        uintptr_t more;
        uintptr_t data;
};

void cgo_symbolizer(void *p) {
        struct cgo_symbolizer_args *args = p;
        if (args->pc == 0) {
                return;
        }

        Dwfl_Module *module = dwfl_addrmodule(dwfl, args->pc);
        if (module == NULL) {
                return;
        }

        args->func = dwfl_module_addrname(module, args->pc);
        Dwfl_Line *line = dwfl_module_getsrc(module, args->pc);
        if (line == NULL) {
                return;
        }
        int line_number = 0;
        args->file = dwfl_lineinfo(line, NULL, &line_number, NULL, NULL, NULL);
        args->lineno = (uintptr_t) line_number;
}
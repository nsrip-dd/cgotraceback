#include <cstring>
#include <cxxabi.h>
#include <stdlib.h>

extern "C" {

char *cgo_traceback_demangle(const char *mangled_name, char *buf, size_t *n, int *status) {
        return abi::__cxa_demangle(mangled_name, buf, n, status);
}

int cgo_traceback_is_mangled(const char *func) {
        return (func != NULL) && (strlen(func) >= 2) && (func[0] == '_') && (func[1] == 'Z');
}

}
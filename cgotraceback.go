//go:build cgo && (linux || darwin)

// package cgotraceback enables collecting call stacks in C code and mapping C
// code instruction addresses to file & function names. This pacakge implements
// the callbacks specified by runtime.SetCgoTraceback. To use the package,
// underscore import it anywhere in your program:
//
//	import _ "github.com/nsrip-dd/cgotraceback"
//
// This library requires libunwind. Either LLVM libunwind or nongnu libunwind
// can be used. libunwind is available by default on macos. It is also available
// for most Linux distributions through their respective package managers, e.g:
//
// 	Alpine:
//		apk add libunwind-dev
//			or
//		apk add llvm-libunwind-dev
//	Debian/Ubuntu:
//		apt install libunwind-dev
//	CentOS:
//		yum install libunwind-devel
//
// In general, a "development" version of the package is required in order to
// get the libunwind headers, in addition to the library itself.
package cgotraceback

import (
	"runtime"
	"unsafe"
)

/*
#cgo CFLAGS: -g -O2
#cgo linux LDFLAGS: -lunwind -ldl
extern void cgo_context(void *);
extern void cgo_traceback(void *);
extern void cgo_symbolizer(void *);
*/
import "C"

func init() {
	runtime.SetCgoTraceback(0, unsafe.Pointer(C.cgo_traceback), unsafe.Pointer(C.cgo_context), unsafe.Pointer(C.cgo_symbolizer))
}

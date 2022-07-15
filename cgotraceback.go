//go:build cgo && (linux || darwin)
// +build cgo
// +build linux darwin

// package cgotraceback enables collecting call stacks in C code and mapping C
// code instruction addresses to file & function names. This pacakge implements
// the callbacks specified by runtime.SetCgoTraceback. To use the package,
// underscore import it anywhere in your program:
//
//	import _ "github.com/nsrip-dd/cgotraceback"
//
// On Linux, instructions from programs/libraries compiled with DWARF debugging
// information are mapped to function names, files, and line numbers using
// libdwfl from elfutils. The library is available on most package managers:
//
// 	Alpine:
//		apk add elfutils-dev
//	Debian/Ubuntu:
//		apt install libdw-dev
//	CentOS:
//		yum install elfutils-libs
//
// This library requires libunwind. Either LLVM libunwind or nongnu libunwind
// can be used on x86-64. libunwind is available by default on macos, and this
// package works on x86-64 or arm64 for macos. It is also available for most
// Linux distributions through their respective package managers, e.g:
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
//
// Note that on arm64 Linux, this package does not work with the nongnu
// libunwind, but it does work with the LLVM libunwind implementation. To get
// the LLVM libunwind implementation for Debian or Ubuntu, refer to
// https://apt.llvm.org/
package cgotraceback

import (
	"runtime"
	"unsafe"
)

/*
#cgo CFLAGS: -g -O2
#cgo linux LDFLAGS: -lunwind -ldl -ldw
extern void cgo_context(void *);
extern void cgo_traceback(void *);
extern void cgo_symbolizer(void *);
extern void cgo_traceback_internal_set_enabled(int);
*/
import "C"

func init() {
	runtime.SetCgoTraceback(0, unsafe.Pointer(C.cgo_traceback), unsafe.Pointer(C.cgo_context), unsafe.Pointer(C.cgo_symbolizer))
}

// for testing
func setEnabled(status bool) {
	var enabled C.int
	if status {
		enabled = 1
	}
	C.cgo_traceback_internal_set_enabled(enabled)
}

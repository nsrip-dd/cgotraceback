//go:build cgo && (linux || darwin)
// +build cgo
// +build linux darwin

// package cgotraceback enables collecting call stacks in C code and mapping C
// code instruction addresses to file & function names. This package implements
// the callbacks specified by runtime.SetCgoTraceback. To use the package,
// underscore import it anywhere in your program:
//
//	import _ "github.com/nsrip-dd/cgotraceback"
//
// By default, dladdr will be used to symbolize instruction addresses. This will
// give function names and the files (i.e. shared libraries or executable) the
// functions are in, but will not have source file names or line numbers.
//
// On Linux, instructions from programs/libraries compiled with DWARF debugging
// information can be mapped to function names, files, and line numbers using
// libdwfl from elfutils. The library is available on most package managers:
//
// 	Alpine:
//		apk add elfutils-dev
//	Debian/Ubuntu:
//		apt install libdw-dev
//	CentOS:
//		yum install elfutils-libs
//
// To use libdwfl, provide the "use_libdwfl" build tag.
package cgotraceback

import (
	"runtime"
	"unsafe"

	asyncprofiler "github.com/nsrip-dd/cgotraceback/internal/async-profiler"
)

/*
#cgo CFLAGS: -g -O2
#cgo CXXFLAGS: -g -O2
#cgo linux LDFLAGS: -ldl
#cgo use_libdwfl LDFLAGS: -ldw
extern void cgo_symbolizer(void *);
*/
import "C"

func init() {
	runtime.SetCgoTraceback(0,
		asyncprofiler.CgoTraceback,
		asyncprofiler.CgoContext,
		unsafe.Pointer(C.cgo_symbolizer),
	)
}

// for testing
func setEnabled(status bool) {
	asyncprofiler.SetEnabled(status)
}

//go:build cgo && (linux || darwin)
// +build cgo
// +build linux darwin

package cgotraceback_test

import (
	"runtime"
	"testing"

	_ "github.com/nsrip-dd/cgotraceback"
	"github.com/nsrip-dd/cgotraceback/internal"
)

func TestCgoTraceback(t *testing.T) {
	var pcs []uintptr
	internal.DoCallback(func() {
		internal.DoCallback2(func() {
			var pc [128]uintptr
			n := runtime.Callers(0, pc[:])
			pcs = pc[:n]
		})
	})

	if len(pcs) == 0 {
		t.Fatalf("no calls in stack")
	}
	frames := runtime.CallersFrames(pcs)
	var got []string
	var found1, found2 bool
	for {
		frame, ok := frames.Next()
		if !ok {
			break
		}
		function := frame.Function
		if function == internal.CFuncName {
			found1 = true
		}
		if function == internal.CFuncName2 {
			found2 = true
		}
		got = append(got, function)
	}
	if !(found1 && found2) {
		t.Log("did not find desired function, got functions:")
	}
	for _, function := range got {
		t.Logf("\t%s", function)
	}
	t.Log("got pcs:")
	for _, pc := range pcs {
		t.Logf("\t%016x", pc)
	}
	if !(found1 && found2) {
		t.FailNow()
	}
}

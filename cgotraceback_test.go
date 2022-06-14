//go:build cgo && (linux || darwin)
// +build cgo
// +build linux darwin

package cgotraceback_test

import (
	"io"
	"reflect"
	"runtime"
	"runtime/pprof"
	"testing"
	"time"

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

func TestRepeatedUnwind(t *testing.T) {
	pcs := make([][]uintptr, 2)
	internal.DoCallback(func() {
		internal.DoCallback2(func() {
			var pc [128]uintptr
			for i := 0; i < 2; i++ {
				n := runtime.Callers(0, pc[:])
				pcs[i] = pc[:n]
			}
		})
	})

	if !reflect.DeepEqual(pcs[0], pcs[1]) {
		t.Errorf("unwound twice with different results")
		t.Logf("first: %x", pcs[0])
		t.Logf("second: %x", pcs[1])
	}
}

// If the libunwind implementation is not signal-safe, then this test might
// induce a deadlock when run with the CPU profiler enabled.
func TestNoDeadlock(t *testing.T) {
	if testing.Short() {
		t.Skip("skipping long test")
	}
	runtime.SetCPUProfileRate(1000)
	err := pprof.StartCPUProfile(io.Discard)
	if err == nil {
		// If CPU profiling was already started (e.g. running benchmarks
		// with the CPU profile) then we don't need to stop it for this
		// test
		defer pprof.StopCPUProfile()
	}
	var pcs []uintptr
	start := time.Now()
	for time.Since(start) < 10*time.Second {
		internal.DoCallback(func() {
			var pc [128]uintptr
			n := runtime.Callers(0, pc[:])
			pcs = pc[:n]
		})
	}
	_ = pcs
}

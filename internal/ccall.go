package internal

/*
#cgo CFLAGS: -g -O0
extern void goCallback(void);
extern void goCallback2(void);

void doGoCallbackRecursive(int depth);
__attribute__ ((noinline)) void doGoCallback(void) {
	doGoCallbackRecursive(10);
}

void doGoCallbackRecursive(int depth) {
	if (depth > 0) {
		doGoCallbackRecursive(depth - 1);
		return;
	}
	goCallback();
}

__attribute__ ((noinline)) void doGoCallback2(void) {
	goCallback2();
}
*/
import "C"

var (
	CFuncName  = "goCallback"
	CFuncName2 = "goCallback2"
)

var callback func()

func DoCallback(f func()) {
	callback = f
	C.doGoCallback()
}

func DoCallback2(f func()) {
	callback = f
	C.doGoCallback2()
}

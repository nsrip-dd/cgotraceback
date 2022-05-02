package internal

import "C"

//export goCallback
func goCallback() {
	callback()
}

//export goCallback2
func goCallback2() {
	callback()
}

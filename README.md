`cgotraceback` enables collecting call stacks in C code and mapping C code
instruction addresses to file & function names. This package implements the
callbacks specified by [`runtime.SetCgoTraceback`](https://pkg.go.dev/runtime#SetCgoTraceback).
To use the package, underscore import it anywhere in your program:

```go
import _ "github.com/nsrip-dd/cgotraceback"
```

By default, [`dladdr`](https://man7.org/linux/man-pages/man3/dladdr1.3.html)
will be used to symbolize instruction addresses. This will give function names
and the files (i.e. shared libraries or executable) the functions are in, but
will not have source file names or line numbers.

On Linux, instructions from programs/libraries compiled with DWARF debugging
information can be mapped to function names, files, and line numbers using
`libdwfl` from [`elfutils`](https://sourceware.org/elfutils/). The library is
available on most package managers:

* Alpine:
	`apk add elfutils-dev`
* Debian/Ubuntu:
	`apt install libdw-dev`
* CentOS:
	`yum install elfutils-libs`

To use `libdwfl`, provide the `use_libdwfl` build tag:

```
$ go build -tags=use_libdwfl
```
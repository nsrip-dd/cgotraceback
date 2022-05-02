#include <stddef.h>

#define _GNU_SOURCE // for dladdr
#include <dlfcn.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>

struct cgo_context {
        unw_context_t unw_ctx; // TODO: Do we need to keep this?
        unw_cursor_t unw_cursor;
        int inuse;
};

static inline int cgo_context_init(struct cgo_context *ctx) {
        int status = unw_getcontext(&ctx->unw_ctx);
        if (status != 0) {
                return status;
        }
        return unw_init_local(&ctx->unw_cursor, &ctx->unw_ctx);
}

// There may be multiple C->Go transitions for a single C tread, so we have a
// per-thread free list of contexts.
//
// Thread-local storage for the context list is safe. A context will be taken
// from the list when a C thread transitions to Go, and that context will be
// released as soon as the Go call returns. Thus the thread that the context
// came from will be alive the entire time the context is in use.
#define cgo_contexts_length 256
static __thread struct cgo_context cgo_contexts[cgo_contexts_length];

static struct cgo_context *cgo_context_get(void) {
        for (int i = 0; i < cgo_contexts_length; i++) {
                if (cgo_contexts[i].inuse == 0) {
                        cgo_contexts[i].inuse = 1;
                        return &cgo_contexts[i];
                }
        }
        return NULL;
}

static void cgo_context_release(struct cgo_context *c) {
        c->inuse = 0;
}

void cgo_context(void *p) {
        struct { uintptr_t p; } *arg = p;
        struct cgo_context *ctx = (struct cgo_context *) arg->p;
        if (ctx != NULL) {
                cgo_context_release(ctx);
                return;
        }
        ctx = cgo_context_get();
        if (ctx == NULL) {
                return;
        }
        if (cgo_context_init(ctx) != 0) {
                return;
        }

        // We have to advance the unwind state up a frame (or a few) before we
        // leave this function. Otherwise the cursor starts at a stack frame
        // that's about to no longer exist. Subsequent function calls can
        // overwrite whatever was at the "top" of the stack frame before.
        //
        // The question is, how far to advance? If we have a call tree that
        // temporarily branches to call this function before continuing on the C
        // thread, where's the ancestor?
        //
        //      +---+---+---> cgo_context
        //      |
        // -----+ <== exported C->Go wrapper function
        //      |
        //      +---+---+---> runtime.cgocallback ---> Go code
        //
        // The answer *SHOULD* be 2. Here's a snippet of code generated by the
        // functions in src/cmd/cgo/out.go for cross-calling a Go function
        // exported as goCallback:
        //
        // void goCallback()
        // {
        //         __SIZE_TYPE__ _cgo_ctxt = _cgo_wait_runtime_init_done();
        //         struct {
        //                 char unused;
        //         } __attribute__((__packed__, __gcc_struct__)) a;
        //         _cgo_tsan_release();
        //         crosscall2(_cgoexp_c5b11aae1618_goCallback, &a, 0, _cgo_ctxt);
        //         _cgo_tsan_acquire();
        //         _cgo_release_context(_cgo_ctxt);
        // }
        //
        // The _cgo_wait_runtime_init_done function does what the name says, and
        // also calls the user-provided cgo context function if it's defined.
        // The return value is the context from the user.
        //
        // Now, we need to get to the goCallback frame in this example, and from
        // there we go into crosscall2 and on to Go. The question is, how deep
        // is the cgo_context call in the _cgo_wait_runtime_init_done call
        // stack? The answer for the current implementation is 1 call:
        //
        // (see src/runtime/cgo/gcc_libinit.c)
        // uintptr_t
        // _cgo_wait_runtime_init_done(void) {
        // 	void (*pfn)(struct context_arg*);
        //
        // 	pthread_mutex_lock(&runtime_init_mu);
        // 	while (runtime_init_done == 0) {
        // 		pthread_cond_wait(&runtime_init_cond, &runtime_init_mu);
        // 	}
        //
        // 	pfn = cgo_context_function;
        //
        // 	pthread_mutex_unlock(&runtime_init_mu);
        // 	if (pfn != nil) {
        // 		struct context_arg arg;
        //
        // 		arg.Context = 0;
        // 		(*pfn)(&arg);
        // 		return arg.Context;
        // 	}
        // 	return 0;
        // }
        // 
        // There are two frames in the call stack we need to skip so we can
        // correctly resume later. The first is this function, and the second is
        // _cgo_wait_runtime_init_done. The next frame after that is the
        // exported C->Go function, which is where unwinding should begin for
        // this context in the traceback function.
        #define CGO_CONTEXT_CALL_STACK_DEPTH 2
        for (int i = 0; i < CGO_CONTEXT_CALL_STACK_DEPTH; i++) {
                if (unw_step(&ctx->unw_cursor) < 0) {
                        return;
                }
        }

        arg->p = (uintptr_t) ctx;
}

struct cgo_traceback_arg {
	uintptr_t  context;
	uintptr_t  sig_context;
	uintptr_t* buf;
	uintptr_t  max;
};

void cgo_traceback(void *p) {
        struct cgo_traceback_arg *arg = p;

        struct cgo_context *ctx = NULL;
        if (arg->context != 0) {
                ctx = (struct cgo_context *) arg->context;
        } else {
                // With no context, we were probably called from a signal
                // handler interrupting a C call.
                struct cgo_context new_ctx;
                cgo_context_init(&new_ctx);
                ctx = &new_ctx;

                // TODO: as in the benesch cgosymbolizer, advance the cursor a
                // few frames to get to the C call that was interrupted? There
                // should be a sigtramp function, which bounces to
                // x_cgo_callers, which calls cgo_traceback.
        }

        int i = 0;
        while (i < arg->max) {
                unw_word_t pc = 0;
                unw_get_reg(&ctx->unw_cursor, UNW_REG_IP, &pc);
                arg->buf[i++] = pc;

                if (unw_step(&ctx->unw_cursor) <= 0) {
                        break;
                }
        }
        if (i < arg->max) {
                // PC 0 indicates the end of the call stack
                arg->buf[i] = 0;
        }
}

void cgo_symbolizer(void* p) {
        struct {
                uintptr_t pc;
                const char* file;
                uintptr_t lineno;
                const char* func;
                uintptr_t entry;
                uintptr_t more;
                uintptr_t data;
        }* arg = p;

        Dl_info dlinfo;
        if (dladdr((void *) arg->pc, &dlinfo) == 0) {
                arg->file = "?";
                arg->func = "?";
                arg->entry = 0;
                return;
        }
        arg->file = dlinfo.dli_fname;
        arg->func = dlinfo.dli_sname;
        arg->entry = (uintptr_t) dlinfo.dli_saddr;
}
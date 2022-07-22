#include <cstring>
#include <ucontext.h>

#include "codeCache.h"
#include "stackWalker.h"
#include "symbols.h"

struct CodeCacheArraySingleton {
    static CodeCacheArray *getInstance();
    static CodeCacheArray *instance;
};

CodeCacheArray *CodeCacheArraySingleton::instance = nullptr;
CodeCacheArray *CodeCacheArraySingleton::getInstance() {
    // XXX(nick): I don't know that I need to care about concurrency. This
    // should be read-only once init() is called.
    if (instance == nullptr) {
        instance = new CodeCacheArray();
    }
    return instance;
}

static __attribute__((constructor)) void init(void) {
    auto a = CodeCacheArraySingleton::getInstance();
	Symbols::parseLibraries(a, false);
}

extern "C"  {

int async_profiler_backtrace(void* ucontext, const void **callchain, int max, int skip) {
    // see Profiler::getNativeTrace
    CodeCacheArray *cache = (CodeCacheArraySingleton::getInstance());
    return StackWalker::walkDwarf(cache, ucontext, callchain, max, skip);
}

static int enabled = 1;

// for benchmarking
void async_cgo_traceback_internal_set_enabled(int value) {
    enabled = value;
}

#define STACK_MAX 32

struct cgo_context {
    uintptr_t stack[STACK_MAX];
    int cached;
    int inuse;
};

// There may be multiple C->Go transitions for a single C tread, so we have a
// per-thread free list of contexts.
//
// Thread-local storage for the context list is safe. A context will be taken
// from the list when a C thread transitions to Go, and that context will be
// released as soon as the Go call returns. Thus the thread that the context
// came from will be alive the entire time the context is in use.
#define cgo_contexts_length 256
static __thread struct cgo_context cgo_contexts[cgo_contexts_length];

// XXX: The runtime.SetCgoTraceback docs claim that cgo_context can be called
// from a signal handler. I know in practice that doesn't happen but maybe it
// could in the future. If so, can we make sure that accessing this list of
// cgo_contexts is signal safe?

static struct cgo_context *cgo_context_get(void) {
    for (int i = 0; i < cgo_contexts_length; i++) {
        if (cgo_contexts[i].inuse == 0) {
            cgo_contexts[i].inuse = 1;
            cgo_contexts[i].cached = 0;
            return &cgo_contexts[i];
        }
    }
    return NULL;
}

static void cgo_context_release(struct cgo_context *c) {
    c->inuse = 0;
}

struct cgo_context_arg {
    uintptr_t p;
};

void async_cgo_context(void *p) {
    if (enabled == 0) {
        return;
    }

    cgo_context_arg *arg = (cgo_context_arg *)p;
    struct cgo_context *ctx = (struct cgo_context *) arg->p;
    if (ctx != NULL) {
        cgo_context_release(ctx);
        return;
    }
    ctx = cgo_context_get();
    if (ctx == NULL) {
        return;
    }

    // There are two frames in the call stack we should skip.  The first is this
    // function, and the second is _cgo_wait_runtime_init_done, which calls this
    // function to save the C call stack context before calling into Go code.
    // The next frame after that is the exported C->Go function, which is where
    // unwinding should begin for this context in the traceback function.
    void *buf[STACK_MAX + 2];
    memset(buf, 0, sizeof(buf));
    int n = async_profiler_backtrace(nullptr, (const void **) ctx->stack, STACK_MAX, 2);
    if (n < STACK_MAX) {
        ctx->stack[n] = 0;
    }
    ctx->cached = 1;
    arg->p = (uintptr_t) ctx;
    return;
}

struct cgo_traceback_arg {
	uintptr_t  context;
	uintptr_t  sig_context;
	uintptr_t* buf;
	uintptr_t  max;
};

void async_cgo_traceback(void *p) {
    if (enabled == 0) {
        return;
    }

    struct cgo_traceback_arg *arg = (struct cgo_traceback_arg *)p;
    struct cgo_context *ctx = NULL;

    // If we had a previous context, then we're being called to unwind some
    // previous C portion of a mixed C/Go call stack. We use the call stack
    // information saved in the context.
    if (arg->context != 0) {
        ctx = (struct cgo_context *) arg->context;
        uintptr_t n = (arg->max < STACK_MAX) ? arg->max : STACK_MAX;
        memcpy(arg->buf, ctx->stack, n * sizeof(uintptr_t));
        return;
    }

    // Otherwise, with no context, this function is being asked to unwind C
    // function calls at the leaf/tail of the call stack (e.g. from a signal
    // handler that just interrupted a C function call). We should skip 3 frames
    // (this function, x_cgo_callers, and runtime.cgoSigtramp)
    ucontext_t *uc = (ucontext_t *) arg->sig_context;
    int n = async_profiler_backtrace(uc, (const void **) arg->buf, arg->max, 0);
    if (n < arg->max) {
        arg->buf[n] = 0;
    }
    return;
}

} // extern "C"
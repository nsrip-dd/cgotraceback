#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <ucontext.h>

#include "stackFrame.h"

static struct sigaction oldact;

#ifdef __APPLE__
#  define REG(l, m)  uc->uc_mcontext->__ss.__##m
#  define SYMBOL(s) _ ## s
#else
#  define REG(l, m)  uc->uc_mcontext.gregs[REG_##l]
#endif

#include "safeAccess.h"

namespace SafeAccess {

NOINLINE __attribute__((aligned(16))) void* load(void** ptr) {
    return *ptr;
}

}

static void segv_handler(int sig, siginfo_t *si, void *ucontext) {
    ucontext_t *uc = (ucontext_t *)ucontext;
    StackFrame frame(uc);

    // If we segfault in the SafeAccess::load, skip past the bad access and
    // set the return value to 0.
    //
    // We have to check if we are *near* the beginning of load, since there will
    // be a few instructions (for frame pointer setup) before the actual bad
    // access
    if ((frame.pc() - (uintptr_t) SafeAccess::load) < 16) {
        uintptr_t instructionEncodedLength = SafeAccess::skipFaultInstruction(frame.pc());
        frame.pc() += instructionEncodedLength;
        frame.retval() = 0x0;
        return;
    }

    // fall back otherwise
    if (oldact.sa_sigaction != nullptr) {
        oldact.sa_sigaction(sig, si, ucontext);
    } else if (oldact.sa_handler != nullptr) {
        oldact.sa_handler(sig);
    } else {
        // If there wasn't a fallback, re-set to the default handler
        // (which just aborts the program) and re-raise the signal
        struct sigaction sa;
        memset(&sa, 0, sizeof(struct sigaction));
        sa.sa_handler = SIG_DFL;
        sigaction(sig, &sa, nullptr);
        raise(sig);
    }
}

__attribute__ ((constructor)) static void init(void) {
    struct sigaction sa;
    memset(&oldact, 0, sizeof(struct sigaction));
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_sigaction = segv_handler;
    sa.sa_flags = SA_SIGINFO;

    sigaction(SIGSEGV, &sa, &oldact);
}
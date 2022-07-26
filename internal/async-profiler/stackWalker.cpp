/*
 * Copyright 2022 Nick Ripley
 * Copyright 2021 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Modified by Nick Ripley to extract components needed for call stack unwinding
 */
#include "codeCache.h"
#include "stackWalker.h"
#include "dwarf.h"
#include "safeAccess.h"
#include "stackFrame.h"

const intptr_t MIN_VALID_PC = 0x1000;
const intptr_t MAX_WALK_SIZE = 0x100000;
const intptr_t MAX_FRAME_SIZE = 0x40000;

static CodeCache *findLibraryByAddress(CodeCacheArray *cache, const void* address) {
    const int native_lib_count = cache->count();
    for (int i = 0; i < native_lib_count; i++) {
        if (cache->operator[](i)->contains(address)) {
            return cache->operator[](i);
        }
    }
    return NULL;
}

bool stepStackContext(StackContext &sc, FrameDesc *f);
bool stepStackContext(StackContext &sc, CodeCacheArray *cache) {
    FrameDesc* f;
    CodeCache* cc = findLibraryByAddress(cache, sc.pc);
    if (cc == NULL || (f = cc->findFrameDesc(sc.pc)) == NULL) {
        f = &FrameDesc::default_frame;
    }
    return stepStackContext(sc, f);
}

bool stepStackContext(StackContext &sc, FrameDesc *f) {
    uintptr_t bottom = sc.sp + MAX_WALK_SIZE;
    uintptr_t prev_sp = sc.sp;

    u8 cfa_reg = (u8)f->cfa;
    int cfa_off = f->cfa >> 8;
    if (cfa_reg == DW_REG_SP) {
        sc.sp = sc.sp + cfa_off;
    } else if (cfa_reg == DW_REG_FP) {
        sc.sp = sc.fp + cfa_off;
    } else if (cfa_reg == DW_REG_PLT) {
        sc.sp += ((uintptr_t)sc.pc & 15) >= 11 ? cfa_off * 2 : cfa_off;
    } else {
        return false;
    }

    // Check if the next frame is below on the current stack
    if (sc.sp < prev_sp || sc.sp >= prev_sp + MAX_FRAME_SIZE || sc.sp >= bottom) {
        return false;
    }

    // Stack pointer must be word aligned
    if ((sc.sp & (sizeof(uintptr_t) - 1)) != 0) {
        return false;
    }

    if (f->fp_off & DW_PC_OFFSET) {
        sc.pc = (const char*)sc.pc + (f->fp_off >> 1);
    } else {
        // XXX(nick): the "SafeAccess" stuff here is used to handle cases
        // where unwinding leads to accessing invalid memory (such as doing
        // frame pointer unwinding through functions which weren't compiled
        // with frame pointers). The call here isn't enough to make it
        // "safe", the async profiler also installs a SEGV signal handler
        // and uses another function to jump over the bad access.
        //
        // I don't know that I want to do that. Would it skip over EVERY bad
        // access?
        if (f->fp_off != DW_SAME_FP && f->fp_off < MAX_FRAME_SIZE && f->fp_off > -MAX_FRAME_SIZE) {
            sc.fp = (uintptr_t)SafeAccess::load((void**)(sc.sp + f->fp_off));
        }
        sc.pc = stripPointer(SafeAccess::load((void**)sc.sp - 1));
    }

    if (sc.pc < (const void*)MIN_VALID_PC || sc.pc > (const void*)-MIN_VALID_PC) {
        return false;
    }
    return true;
}

void populateStackContext(StackContext &sc, void *ucontext) {
    if (ucontext == NULL) {
        sc.pc = __builtin_return_address(0);
        sc.fp = (uintptr_t)__builtin_frame_address(1); // XXX(nick): this isn't safe....
        sc.sp = (uintptr_t)__builtin_frame_address(0);
    } else {
        StackFrame frame(ucontext);
        sc.pc = (const void*)frame.pc();
        sc.fp = frame.fp();
        sc.sp = frame.sp();
    }
}

int stackWalk(CodeCacheArray *cache, StackContext &sc, const void** callchain, int max_depth, int skip);
int stackWalk(CodeCacheArray *cache, void* ucontext, const void** callchain, int max_depth, int skip) {
    StackContext sc;
    populateStackContext(sc, ucontext);
    return stackWalk(cache, sc, callchain, max_depth, skip);
}

int stackWalk(CodeCacheArray *cache, StackContext &sc, const void** callchain, int max_depth, int skip) {
    int depth = -skip;

    // Walk until the bottom of the stack or until the first Java frame
    while (depth < max_depth) {
        int d = depth++;
        if (d >= 0) {
            callchain[d] = sc.pc;
        }
        if (!stepStackContext(sc, cache)) {
	        break;
        }
    }

    return depth;
}

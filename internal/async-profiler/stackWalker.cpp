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

int StackWalker::walkDwarf(CodeCacheArray *cache, void* ucontext, const void** callchain, int max_depth, int skip) {
    const void* pc;
    uintptr_t fp;
    uintptr_t sp;
    uintptr_t prev_sp;
    if (ucontext == NULL) {
        pc = __builtin_return_address(0);
        fp = (uintptr_t)__builtin_frame_address(1); // XXX(nick): this isn't safe....
        sp = (uintptr_t)__builtin_frame_address(0);
    } else {
        StackFrame frame(ucontext);
        pc = (const void*)frame.pc();
        fp = frame.fp();
        sp = frame.sp();
    }
    // We flag the "bottom" of the stack as a simple guard against unwinding too
    // far. We need to compute the bottom relative to whichever stack pointer
    // we're actually unwinding against as opposed to the stack this function is
    // called on. This is necessary because Go provides its own stack for
    // SIGPROF handling through sigaltstack, so this function will run on a
    // different stack than the one we're interrupting during CPU profiling.
    //
    // TODO: find a more stable/safe way to compute the bottom of the stack?
    uintptr_t bottom = sp + MAX_WALK_SIZE;

    int depth = -skip;

    // Walk until the bottom of the stack or until the first Java frame
    while (depth < max_depth) {
        int d = depth++;
        if (d >= 0) {
            callchain[d] = pc;
        }
        prev_sp = sp;

        FrameDesc* f;
        CodeCache* cc = findLibraryByAddress(cache, pc);
        if (cc == NULL || (f = cc->findFrameDesc(pc)) == NULL) {
            f = &FrameDesc::default_frame;
        }

        u8 cfa_reg = (u8)f->cfa;
        int cfa_off = f->cfa >> 8;
        if (cfa_reg == DW_REG_SP) {
            sp = sp + cfa_off;
        } else if (cfa_reg == DW_REG_FP) {
            sp = fp + cfa_off;
        } else if (cfa_reg == DW_REG_PLT) {
            sp += ((uintptr_t)pc & 15) >= 11 ? cfa_off * 2 : cfa_off;
        } else {
            break;
        }

        // Check if the next frame is below on the current stack
        if (sp < prev_sp || sp >= prev_sp + MAX_FRAME_SIZE || sp >= bottom) {
            break;
        }

        // Stack pointer must be word aligned
        if ((sp & (sizeof(uintptr_t) - 1)) != 0) {
            break;
        }

        if (f->fp_off & DW_PC_OFFSET) {
            pc = (const char*)pc + (f->fp_off >> 1);
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
                fp = (uintptr_t)SafeAccess::load((void**)(sp + f->fp_off));
            }
            pc = stripPointer(SafeAccess::load((void**)sp - 1));
        }

        if (pc < (const void*)MIN_VALID_PC || pc > (const void*)-MIN_VALID_PC) {
            break;
        }
    }

    return depth;
}

/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _WASM_INTERP_H
#define _WASM_INTERP_H

#include "wasm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct WASMModuleInstance;
struct WASMFunctionInstance;

typedef struct WASMInterpFrame {
  /* The frame of the caller that are calling the current function. */
  struct WASMInterpFrame *prev_frame;

  /* The current WASM function. */
  struct WASMFunctionInstance *function;

  /* Instruction pointer of the bytecode array.  */
  uint8 *ip;

  /* Operand stack top pointer of the current frame.  The bottom of
     the stack is the next cell after the last local variable.  */
  uint32 *sp_bottom;
  uint32 *sp_boundary;
  uint32 *sp;

  WASMBranchBlock *csp_bottom;
  WASMBranchBlock *csp_boundary;
  WASMBranchBlock *csp;

  /* Frame data, the layout is:
     lp: param_cell_count + local_cell_count
     sp_bottom to sp_boundary: stack of data
     csp_bottom to csp_boundary: stack of block
     ref to frame end: data types of local vairables and stack data
     */
  uint32 lp[1];
} WASMInterpFrame;

/**
 * Calculate the size of interpreter area of frame of a function.
 *
 * @param all_cell_num number of all cells including local variables
 * and the working stack slots
 *
 * @return the size of interpreter area of the frame
 */
static inline unsigned
wasm_interp_interp_frame_size(unsigned all_cell_num)
{
  return align_uint(offsetof(WASMInterpFrame, lp) + all_cell_num * 5, 4);
}

void
wasm_interp_call_wasm(struct WASMModuleInstance *module_inst,
                      struct WASMFunctionInstance *function,
                      uint32 argc, uint32 argv[]);

#ifdef __cplusplus
}
#endif

#endif /* end of _WASM_INTERP_H */

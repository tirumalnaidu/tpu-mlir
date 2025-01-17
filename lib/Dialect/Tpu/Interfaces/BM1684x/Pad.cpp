//===----------------------------------------------------------------------===//
//
// Copyright (c) 2020-2030 by Sophgo Technologies Inc. All rights reserved.
//
// Licensed under the Apache License v2.0.
// See http://www.apache.org/licenses/LICENSE-2.0 for license information.
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Backend/BM168x/BM1684x.h"
#include "tpu_mlir/Dialect/Tpu/IR/TpuOps.h"
#include "tpu_mlir/Support/Helper/Module.h"
#include "tpu_mlir/Support/Helper/Quant.h"
#include "tpu_mlir/Support/MathUtils.h"

using namespace mlir;
using namespace tpu_mlir;
using namespace tpu_mlir::helper;
using namespace tpu_mlir::backend;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  bool is_local;
  unsigned long long input_addr;
  unsigned long long output_addr;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int pad[4][2];
  int type;
  float constant;
  DATA_TYPE_T dtype;
} pad_param_t;

#ifdef __cplusplus
}
#endif

// =========================================
// GlobalGenInterface
// =========================================
void tpu::PadOp::codegen_global_int8_bm1684x() {
  pad_param_t param = {0};
  param.is_local = false;
  param.input_addr = Module::getAddress(input());
  param.output_addr = Module::getAddress(output());
  auto in_shape = Module::getShape(input());
  int pad_dim = in_shape.size();
  param.input_n = in_shape[0];
  param.input_c = in_shape[1];
  param.input_h = in_shape[2];
  param.input_w = in_shape[3];
  auto pads = Module::getI64Array(paddings());
  for (int i = 0; i < pad_dim; i++) {
    param.pad[i][0] = pads->at(i);
    param.pad[i][1] = pads->at(i + pad_dim);
  }
  param.type = 0;
  param.constant = 0;
  param.dtype = BM168x::getDataType(input());

  BM1684x::instance().call_global_func("backend_api_pad", &param,
                                       sizeof(param));
}

void tpu::PadOp::codegen_global_float_bm1684x() {
  codegen_global_int8_bm1684x();
}

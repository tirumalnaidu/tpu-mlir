#include "sophgo/Dialect/Tpu/IR/TpuOps.h"
#include "sophgo/Backend/BM168x/BM1686.h"
#include "sophgo/Support/Helper/Quant.h"
#include "sophgo/Support/Helper/Module.h"

using namespace mlir;
using namespace sophgo;
using namespace sophgo::helper;
using namespace sophgo::backend;

typedef struct {
  /* common param of float and fixed */
  unsigned long long L_addr;
  unsigned long long R_addr;
  unsigned long long bias_addr;
  unsigned long long rzp_addr;
  unsigned long long Y_addr;
  int L_row_num;
  int L_col_num;
  int R_col_num;
  int R_transpose;
  int have_bias;
  int L_dtype;
  int R_dtype;
  int Y_dtype;
  int if_relu;
  float relu_upper_limit;
  /* quantize param */
  int bias_dtype;
  int rshift;
  bool is_asymmetric;
  int rzp_const_val;
  bool rzp_is_const;
  /* requantize param */
  int requant_mode; // mode < 0 means no requantize
  int mul_val;
  int shift_val;
  int offset_val;
} fc_global_param_t;

void tpu::MatMulOp::codegen_global_int8_bm1686() {
  int64_t batch, M, K, N;
  bool with_bias, relu;
  parseParam(batch, M, K, N, with_bias, relu);
  assert(batch == 1);

  fc_global_param_t param = {0};
  param.L_row_num = M;
  param.L_col_num = K;
  param.R_col_num = N;
  param.have_bias = with_bias;
  param.R_transpose = 0;
  param.is_asymmetric = true;
  param.rzp_is_const = true;
  param.rzp_const_val = 0;
  param.L_addr = Module::getAddress(input());
  param.R_addr = Module::getAddress(right());
  param.Y_addr = Module::getAddress(output());
  if (param.have_bias) {
    param.bias_addr = Module::getAddress(bias());
    param.bias_dtype = BM168x::getDataType(bias());
  }
  param.L_dtype = BM168x::getDataType(input());
  param.R_dtype = BM168x::getDataType(right());
  param.Y_dtype = DTYPE_INT8;
  param.if_relu = relu;
  param.relu_upper_limit = 0;
  param.rshift = 0;
  param.requant_mode = 1;
  param.mul_val = multiplier();
  param.shift_val = rshift();
  auto output_type =
      Quant::getUniformQuantizedType(output());
  param.offset_val = output_type.getZeroPoint();
  BM1686::instance().call_global_func("backend_api_fc", &param,
                                      sizeof(fc_global_param_t));
}

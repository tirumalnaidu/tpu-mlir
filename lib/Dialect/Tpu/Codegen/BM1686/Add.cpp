#include "sophgo/Dialect/Tpu/IR/TpuOps.h"
#include "sophgo/Backend/BM168x/BM1686.h"
#include "sophgo/Support/Helper/Quant.h"
#include "sophgo/Support/Helper/Module.h"

using namespace mlir;
using namespace sophgo;
using namespace sophgo::helper;
using namespace sophgo::backend;

typedef struct {
  unsigned long long input_A_global_addr;
  unsigned long long input_B_global_addr;
  unsigned long long output_global_addr;
  int n;
  int c;
  int h;
  int w;
  int op_code;
  int scale_A;
  int scale_B;
  int rshift_A;
  int rshift_B;
  int if_relu;
  DATA_TYPE_T dtype_A;
  DATA_TYPE_T dtype_B;
  int round_mode;
} eltwise_fixed_global_param_t;

typedef struct {
  unsigned long long *input_global_addr;
  unsigned long long output_global_addr;
  unsigned long long mask_global_addr;
  int input_num;
  int n;
  int c;
  int h;
  int w;
  int op_code;
  int *coeff;
  int need_mask;
  int *mask_index;
  int if_relu;
  DATA_TYPE_T dtype;
} eltwise_float_global_param_t;

typedef struct {
  unsigned int *input_local_addr;
  unsigned int output_local_addr;
  unsigned int buffer_local_addr;
  int input_num;
  int n;
  int c;
  int h;
  int w;
  int op_code;
  float *coeff;
  int *input_local_cstride;
  int if_relu;
  DATA_TYPE_T dtype;
} eltwise_float_local_param_t;

void tpu::AddOp::codegen_global_int8_bm1686() {
  eltwise_fixed_global_param_t p;
  p.input_A_global_addr = Module::getAddress(inputs()[0]);
  p.input_B_global_addr = Module::getAddress(inputs()[1]);
  p.output_global_addr = Module::getAddress(output());
  int64_t n, c, h, w;
  Module::getNCHW(output(), n, c, h, w);
  p.n = (int)n;
  p.c = (int)c;
  p.h = (int)h;
  p.w = (int)w;
  p.op_code = 1; // (0: Product; 1: Sum; 2: Max)
  auto multipliers_v = Module::getI64Array(multipliers());
  auto rshift_v = Module::getI64Array(rshifts());
  p.scale_A = (int)multipliers_v->at(0);
  p.scale_B = (int)multipliers_v->at(1);
  p.rshift_A = (int)rshift_v->at(0);
  p.rshift_B = (int)rshift_v->at(1);
  p.if_relu = do_relu();
  p.dtype_A = BM168x::getDataType(inputs()[0]);
  p.dtype_B = BM168x::getDataType(inputs()[1]);
  p.round_mode = ROUND_UP;
  BM1686::instance().call_global_func("backend_api_eltwise_fixed_global", &p,
                                      sizeof(eltwise_fixed_global_param_t));
}

typedef struct {
  unsigned int *input_local_addr;
  unsigned int output_local_addr;
  unsigned int buffer_local_addr;
  int n;
  int c;
  int h;
  int w;
  int op_code;
  int *input_local_cstride;
  int *scale_weight;
  int *rshift;
  DATA_TYPE_T *input_dtype;
  int input_num;
  int if_relu;
  int round_mode;
} eltwise_fixed_local_param_t;

void tpu::AddOp::codegen_local_int8_bm1686(int64_t n_step, int64_t h_step) {
  // int bottom_num = layer_in_tensors.size();
  // const TENSOR_PARAM_T *p_tensor_in_param =
  //     net_graph_->get_tensor_param(layer_in_tensors[0]);
  // int depth = p_tensor_in_param->d_slice;
  // DATA_TYPE_T *p_bottom_dtype = new DATA_TYPE_T[bottom_num];
  // for (int i = 0; i < bottom_num; ++i)
  //   p_bottom_dtype[i] = net_graph_->get_tensor_data_type(layer_in_tensors[i]);
  // if (p_bottom_dtype[0] == DTYPE_FP32 || p_bottom_dtype[0] == DTYPE_FP16 ||
  //     p_bottom_dtype[0] == DTYPE_BFP16) {
  //   eltwise_float_local_param_t p;
  //   p.input_local_addr = p_bottom_local_offset_;
  //   p.buffer_local_addr = imm_buffer_local_offset_;
  //   p.output_local_addr = top_local_offset_;
  //   p.input_num = bottom_num;
  //   p.n = bottom_local_dim_[0] * depth;
  //   p.c = bottom_local_dim_[1];
  //   p.h = bottom_local_dim_[2];
  //   p.w = bottom_local_dim_[3];
  //   p.op_code = layer_param->layer_param_u.eltwise_param.op_code;
  //   p.coeff = (float *)layer_param->layer_param_u.eltwise_param.bottom_coeff;
  //   p.input_local_cstride =
  //       bottom_chstride_en_ ? p_bottom_local_chstride_ : NULL;
  //   p.if_relu = layer_param->if_relu ? 1 : 0;
  //   p.dtype = p_bottom_dtype[0];
  //   call_local_func("backend_api_eltwise_float_local", &p,
  //                   sizeof(eltwise_float_local_param_t));
  // } else if (p_bottom_dtype[0] == DTYPE_INT8 ||
  //            p_bottom_dtype[0] == DTYPE_UINT8) {
  //   eltwise_fixed_local_param_t p;
  //   p.output_local_addr = top_local_offset_;
  //   p.input_local_addr = p_bottom_local_offset_;
  //   p.buffer_local_addr = imm_buffer_local_offset_;
  //   p.n = bottom_local_dim_[0];
  //   p.c = bottom_local_dim_[1] * depth;
  //   p.h = bottom_local_dim_[2];
  //   p.w = bottom_local_dim_[3];
  //   p.op_code = layer_param->layer_param_u.eltwise_param.op_code;
  //   p.input_local_cstride =
  //       bottom_chstride_en_ ? p_bottom_local_chstride_ : NULL;
  //   p.scale_weight =
  //       (int *)layer_param->layer_param_u.eltwise_param.bottom_coeff;
  //   p.rshift = (int *)layer_param->layer_param_u.eltwise_param.in_rshift_num;
  //   p.input_dtype = p_bottom_dtype;
  //   p.input_num = bottom_num;
  //   p.if_relu = layer_param->if_relu ? 1 : 0;
  //   p.round_mode = ROUND_UP;
  //   call_local_func("backend_api_eltwise_fixed_local", &p,
  //                   sizeof(eltwise_fixed_local_param_t));
  // }
  // delete[] p_bottom_dtype;
}

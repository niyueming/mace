//
// Copyright (c) 2017 XiaoMi All rights reserved.
//

#include "mace/kernels/conv_2d.h"
#include "mace/core/common.h"
#include "mace/core/runtime/opencl/opencl_runtime.h"
#include "mace/kernels/activation.h"
#include "mace/kernels/opencl/helper.h"
#include "mace/utils/tuner.h"
#include "mace/utils/utils.h"

namespace mace {
namespace kernels {

static void Conv2d3x3S12(const Tensor *input,
                         const Tensor *filter,
                         const Tensor *bias,
                         const uint32_t stride,
                         const int *padding,
                         const int *dilations,
                         const ActivationType activation,
                         const float relux_max_limit,
                         const float prelu_alpha,
                         const DataType dt,
                         Tensor *output,
                         StatsFuture *future) {
  const index_t batch = output->dim(0);
  const index_t height = output->dim(1);
  const index_t width = output->dim(2);
  const index_t channels = output->dim(3);
  const index_t input_channels = input->dim(3);

  const index_t channel_blocks = RoundUpDiv4(channels);
  const index_t input_channel_blocks = RoundUpDiv4(input_channels);
  const index_t width_blocks = RoundUpDiv<index_t, 5>(width);

  std::set<std::string> built_options;
  std::string kernel_name = MACE_OBFUSCATE_SYMBOL("conv_2d_3x3");
  built_options.emplace("-Dconv_2d_3x3=" + kernel_name);
  built_options.emplace("-DDATA_TYPE=" + DtToUpstreamCLDt(dt));
  built_options.emplace("-DCMD_DATA_TYPE=" + DtToUpstreamCLCMDDt(dt));
  built_options.emplace(bias != nullptr ? "-DBIAS" : "");
  built_options.emplace("-DSTRIDE=" + ToString(stride));
  switch (activation) {
    case NOOP:
      break;
    case RELU:
      built_options.emplace("-DUSE_RELU");
      break;
    case RELUX:
      built_options.emplace("-DUSE_RELUX");
      break;
    case PRELU:
      built_options.emplace("-DUSE_PRELU");
      break;
    case TANH:
      built_options.emplace("-DUSE_TANH");
      break;
    case SIGMOID:
      built_options.emplace("-DUSE_SIGMOID");
      break;
    defeult:
      LOG(FATAL) << "Unknown activation type: " << activation;
  }

  auto runtime = OpenCLRuntime::Global();
  auto conv_2d_kernel =
      runtime->BuildKernel("conv_2d_3x3", kernel_name, built_options);

  uint32_t idx = 0;
  conv_2d_kernel.setArg(idx++,
                        *(static_cast<const cl::Image2D *>(input->buffer())));
  conv_2d_kernel.setArg(idx++,
                        *(static_cast<const cl::Image2D *>(filter->buffer())));
  if (bias != nullptr) {
    conv_2d_kernel.setArg(idx++,
                          *(static_cast<const cl::Image2D *>(bias->buffer())));
  }
  conv_2d_kernel.setArg(idx++,
                        *(static_cast<const cl::Image2D *>(output->buffer())));
  conv_2d_kernel.setArg(idx++, relux_max_limit);
  conv_2d_kernel.setArg(idx++, prelu_alpha);
  conv_2d_kernel.setArg(idx++, static_cast<int>(input->dim(1)));
  conv_2d_kernel.setArg(idx++, static_cast<int>(input->dim(2)));
  conv_2d_kernel.setArg(idx++, static_cast<int>(input_channel_blocks));
  conv_2d_kernel.setArg(idx++, static_cast<int>(height));
  conv_2d_kernel.setArg(idx++, static_cast<int>(width));
  conv_2d_kernel.setArg(idx++, padding[0] / 2);
  conv_2d_kernel.setArg(idx++, padding[1] / 2);
  conv_2d_kernel.setArg(idx++, dilations[0]);
  conv_2d_kernel.setArg(idx++, dilations[1]);

  const uint32_t gws[3] = {static_cast<uint32_t>(channel_blocks),
                           static_cast<uint32_t>(width_blocks),
                           static_cast<uint32_t>(height * batch)};
  const std::vector<uint32_t> lws = {4, 15, 8, 1};
  std::string tuning_key =
      Concat("conv2d_3x3_opencl_kernel_", activation, output->dim(0),
             output->dim(1), output->dim(2), output->dim(3));
  TuningOrRun3DKernel(conv_2d_kernel, tuning_key, gws, lws, future);
}
void Conv2dOpenclK3x3S1(const Tensor *input,
                        const Tensor *filter,
                        const Tensor *bias,
                        const int *padding,
                        const int *dilations,
                        const ActivationType activation,
                        const float relux_max_limit,
                        const float prelu_alpha,
                        const DataType dt,
                        Tensor *output,
                        StatsFuture *future) {
  Conv2d3x3S12(input, filter, bias, 1, padding, dilations, activation,
               relux_max_limit, prelu_alpha, dt, output, future);
};

void Conv2dOpenclK3x3S2(const Tensor *input,
                        const Tensor *filter,
                        const Tensor *bias,
                        const int *padding,
                        const int *dilations,
                        const ActivationType activation,
                        const float relux_max_limit,
                        const float prelu_alpha,
                        const DataType dt,
                        Tensor *output,
                        StatsFuture *future) {
  Conv2d3x3S12(input, filter, bias, 2, padding, dilations, activation,
               relux_max_limit, prelu_alpha, dt, output, future);
};

}  // namespace kernels
}  // namespace mace

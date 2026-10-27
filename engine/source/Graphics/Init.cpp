#include "Graphics/Init.hpp"
#include "Graphics/GpuTimer.hpp"
#include <iostream>
#include <vector>
#include <Mariana/Foundation/Logger.hpp>
#include <dawn/webgpu_cpp_print.h>

wgpu::Instance instance;
wgpu::Adapter adapter;
wgpu::Device device;

wgpu::Surface surface;
wgpu::TextureFormat windowFormat;

wgpu::Limits deviceLimits;

void Init()
{
  static const auto kTimedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
  wgpu::InstanceDescriptor instanceDesc{.requiredFeatureCount = 1,
                                        .requiredFeatures = &kTimedWaitAny};
  instance = wgpu::CreateInstance(&instanceDesc);

  wgpu::Future f1 = instance.RequestAdapter(
      nullptr, wgpu::CallbackMode::WaitAnyOnly,
      [](wgpu::RequestAdapterStatus status, wgpu::Adapter a,
         wgpu::StringView message) {
        if (status != wgpu::RequestAdapterStatus::Success) {
          std::cout << "RequestAdapter: " << message << "\n";
          exit(0);
        }
        adapter = std::move(a);
      });
  instance.WaitAny(f1, UINT64_MAX);

  wgpu::DeviceDescriptor desc{};
  // Timestamp queries time the passes for the profiler (GpuTimer), when the adapter has them.
  std::vector<wgpu::FeatureName> features;
  const bool timestamps = adapter.HasFeature(wgpu::FeatureName::TimestampQuery);
  if (timestamps)
    features.push_back(wgpu::FeatureName::TimestampQuery);
  desc.requiredFeatureCount = features.size();
  desc.requiredFeatures = features.data();
#if !defined(__EMSCRIPTEN__)
  // Dawn rounds timestamps (as browsers do, against timing attacks); natively they can be exact.
  static const char* const kDisabledToggles[] = { "timestamp_quantization" };
  wgpu::DawnTogglesDescriptor toggles;
  toggles.disabledToggleCount = 1;
  toggles.disabledToggles = kDisabledToggles;
  desc.nextInChain = &toggles;
#endif
  desc.SetUncapturedErrorCallback([](const wgpu::Device&,
                                     wgpu::ErrorType errorType,
                                     wgpu::StringView message) {
    std::cout << "Error: " << errorType << " - message: " << message << "\n";
  });

  wgpu::Future f2 = adapter.RequestDevice(
      &desc, wgpu::CallbackMode::WaitAnyOnly,
      [](wgpu::RequestDeviceStatus status, wgpu::Device d,
         wgpu::StringView message) {
        if (status != wgpu::RequestDeviceStatus::Success) {
          std::cout << "RequestDevice: " << message << "\n";
          exit(0);
        }
        device = std::move(d);
      });
  instance.WaitAny(f2, UINT64_MAX);
  
  device.GetLimits(&deviceLimits);
#if defined(__EMSCRIPTEN__)
  GpuTimer::Setup(timestamps, timestamps ? "Passes timed with timestamp queries (the browser rounds them to 0.1 ms "
                                           "unless chrome://flags has WebGPU Developer Features on)."
                                         : "This browser doesn't let WebGPU time passes (no timestamp-query feature).");
#else
  GpuTimer::Setup(timestamps, timestamps ? "Passes timed with timestamp queries." : "This GPU can't time passes (no timestamp queries).");
#endif
  Logger::Info("WEBGPU backend initialized");
}

uint32_t ceilToNextMultiple(uint32_t value)
{
  uint32_t step = deviceLimits.minUniformBufferOffsetAlignment;
  uint32_t divide_and_ceil = value / step + (value % step == 0 ? 0 : 1);
  return step * divide_and_ceil;
}

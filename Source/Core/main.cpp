#include <webgpu/webgpu_cpp.h>
#include "GlobalVaribles.hpp"
#include "Application.hpp"
#include <iostream>

void GetAdapter(void (*callback)(wgpu::Adapter)) {
  #if defined(__EMSCRIPTEN__)
  instance.RequestAdapter(
      nullptr,
      [](WGPURequestAdapterStatus status, WGPUAdapter cAdapter,
         const char* message, void* userdata) {
        if (status != WGPURequestAdapterStatus_Success) {
          exit(0);
        }
        wgpu::Adapter adapter = wgpu::Adapter::Acquire(cAdapter);
        reinterpret_cast<void (*)(wgpu::Adapter)>(userdata)(adapter);
  }, reinterpret_cast<void*>(callback));
  #else
  instance.RequestAdapter(
      nullptr,
      [](WGPURequestAdapterStatus status, WGPUAdapter cAdapter,
         WGPUStringView message, void* userdata) {
        if (status != WGPURequestAdapterStatus_Success) {
          exit(0);
        }
        wgpu::Adapter adapter = wgpu::Adapter::Acquire(cAdapter);
        reinterpret_cast<void (*)(wgpu::Adapter)>(userdata)(adapter);
  }, reinterpret_cast<void*>(callback));
  #endif
}

void GetDevice(void (*callback)(wgpu::Device)) {
  #if defined(__EMSCRIPTEN__)
  adapter.RequestDevice(
      nullptr,
      [](WGPURequestDeviceStatus status, WGPUDevice cDevice,
          const char* message, void* userdata) {
        wgpu::Device device = wgpu::Device::Acquire(cDevice);
        device.SetUncapturedErrorCallback(
            [](WGPUErrorType type, const char* message, void* userdata) {
              std::cout << "Error: " << type << " - message: " << message;
            },
            nullptr);
        reinterpret_cast<void (*)(wgpu::Device)>(userdata)(device);
  }, reinterpret_cast<void*>(callback));
  #else
  adapter.RequestDevice(
      nullptr,
      [](WGPURequestDeviceStatus status, WGPUDevice cDevice,
          WGPUStringView message, void* userdata) {
        wgpu::Device device = wgpu::Device::Acquire(cDevice);
        device.SetUncapturedErrorCallback(
            [](WGPUErrorType type, WGPUStringView message, void* userdata) {
              std::cout << "Error: " << type << " - message: " << message.data;
            },
            nullptr);
        reinterpret_cast<void (*)(wgpu::Device)>(userdata)(device);
  }, reinterpret_cast<void*>(callback));
  #endif
}

void Start()
{
  Application app;
}

int main() {
  std::cout << "Starting app" << std::endl;
  instance = wgpu::CreateInstance();
  GetAdapter([](wgpu::Adapter a) {
    adapter = a;
    GetDevice([](wgpu::Device d) {
      device = d;
      Start();
    });
  });
}
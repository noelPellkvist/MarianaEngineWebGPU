#include "Application.h"
#include <GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>
#include <iostream>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include <webgpu/webgpu_glfw.h>
#endif

wgpu::Instance instance;
wgpu::Adapter adapter;
wgpu::Device device;

void GetAdapter(void (*callback)(wgpu::Adapter)) {
#if defined(__EMSCRIPTEN__)
    instance.RequestAdapter(
        nullptr,
        [](WGPURequestAdapterStatus status, WGPUAdapter cAdapter,
            const char *message, void* userdata) {
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
            [](WGPUErrorType type, const char *message, void* userdata) {
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
    MarianaEngine::Core::ApplicationInfo info = {};
    info.Width = 848;
    info.Height = 480;
    info.adapter = &adapter;
    info.device = &device;
    info.instance = &instance;
    info.name = "Mariana Engine QuadRendering";
    MarianaEngine::Core::Application App;
    App.Init(info);
}

int main() {
    instance = wgpu::CreateInstance();
    
    GetAdapter([](wgpu::Adapter a) {
        adapter = a;

        GetDevice([](wgpu::Device d) {
            device = d;
            Start();
        });
    });
}
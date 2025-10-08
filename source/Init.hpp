#pragma once
#include <webgpu/webgpu_cpp.h>
#include <glm/glm.hpp>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

extern wgpu::Instance instance;
extern wgpu::Adapter adapter;
extern wgpu::Device device;

extern wgpu::Surface surface;
extern wgpu::TextureFormat windowFormat;

extern wgpu::Limits deviceLimits;

void Init();

uint32_t ceilToNextMultiple(uint32_t value);
#pragma once
#include <webgpu/webgpu_cpp.h>

extern wgpu::Instance instance;
extern wgpu::Adapter adapter;
extern wgpu::Device device;

extern wgpu::Surface surface;
extern wgpu::TextureFormat windowFormat;

void Init();
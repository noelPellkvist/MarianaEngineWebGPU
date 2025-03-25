#pragma once
#include <webgpu/webgpu_cpp.h>
#include "Renderer/GUI.hpp"

extern wgpu::Instance instance;
extern wgpu::Adapter adapter;
extern wgpu::Device device;

extern GUI* gui;
#pragma once
#include <webgpu/webgpu_cpp.h>

struct Texture
{
    wgpu::Texture texture;
    wgpu::TextureView textureView;
};
#include <Mariana/Graphics/NativeGPU.hpp>
#include "Graphics/Init.hpp"

namespace NativeGPU
{
    const wgpu::Device& Device()
    {
        return device;
    }

    wgpu::TextureFormat SurfaceFormat()
    {
        return windowFormat;
    }
}

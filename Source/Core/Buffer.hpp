#pragma once
#include <vector>
#include <webgpu/webgpu_cpp.h>

struct BufferEntry
{
    std::vector<uint8_t> data;
};

struct BufferData
{
    std::vector<BufferEntry> entries;
};

wgpu::VertexBufferLayout GetVertexBufferLayout(BufferData bufferData);
wgpu::Buffer UploadVertexBuffer(BufferData bufferData);
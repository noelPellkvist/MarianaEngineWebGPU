#pragma once
#include <vector>
#include <string>
#include <webgpu/webgpu_cpp.h>
#include <map>

#define LayoutEntryType  wgpu::VertexFormat

struct VertexBufferLayoutEntry
{
    std::string name;
    LayoutEntryType type;
};

struct VertexBufferEntry
{
    std::string name;
    LayoutEntryType type;
    size_t offsett;
};

struct VertexAttribute
{
    std::string name;
    void* data;
    size_t numElements;
};

struct VertexBufferLayout
{
    std::vector<VertexBufferLayoutEntry> entries;
};

struct VertexBufferLayoutData
{
    std::vector<wgpu::VertexAttribute> attributes;
    wgpu::VertexBufferLayout vertexBufferLayout;
    std::map<std::string, VertexBufferEntry> offsetMap = {};

    std::vector<std::string> GetRequiredEntries();
};

VertexBufferLayoutData BuildVertexLayout(VertexBufferLayout bufferData);

wgpu::Buffer CreateRawVertexBuffer(VertexBufferLayoutData layout, const std::vector<VertexAttribute>& data);
#include "../Logging.hpp"
#include "../GlobalVaribles.hpp"
#include "VertexBuffer.hpp"
#include <glm.hpp>


#include <utility>

constexpr uint32_t GetVertexFormatSize(LayoutEntryType format) {
    switch (format) {
        case LayoutEntryType::Uint8x2:   return 2 * sizeof(uint8_t);
        case LayoutEntryType::Uint8x4:   return 4 * sizeof(uint8_t);
        case LayoutEntryType::Sint8x2:   return 2 * sizeof(int8_t);
        case LayoutEntryType::Sint8x4:   return 4 * sizeof(int8_t);
        case LayoutEntryType::Unorm8x2:  return 2 * sizeof(uint8_t);
        case LayoutEntryType::Unorm8x4:  return 4 * sizeof(uint8_t);
        case LayoutEntryType::Snorm8x2:  return 2 * sizeof(int8_t);
        case LayoutEntryType::Snorm8x4:  return 4 * sizeof(int8_t);

        case LayoutEntryType::Uint16x2:  return 2 * sizeof(uint16_t);
        case LayoutEntryType::Uint16x4:  return 4 * sizeof(uint16_t);
        case LayoutEntryType::Sint16x2:  return 2 * sizeof(int16_t);
        case LayoutEntryType::Sint16x4:  return 4 * sizeof(int16_t);
        case LayoutEntryType::Unorm16x2: return 2 * sizeof(uint16_t);
        case LayoutEntryType::Unorm16x4: return 4 * sizeof(uint16_t);
        case LayoutEntryType::Snorm16x2: return 2 * sizeof(int16_t);
        case LayoutEntryType::Snorm16x4: return 4 * sizeof(int16_t);

        case LayoutEntryType::Float16x2: return 2 * sizeof(uint16_t);
        case LayoutEntryType::Float16x4: return 4 * sizeof(uint16_t);

        case LayoutEntryType::Float32:   return sizeof(float);
        case LayoutEntryType::Float32x2: return 2 * sizeof(float);
        case LayoutEntryType::Float32x3: return 3 * sizeof(float);
        case LayoutEntryType::Float32x4: return 4 * sizeof(float);

        case LayoutEntryType::Uint32:    return sizeof(uint32_t);
        case LayoutEntryType::Uint32x2:  return 2 * sizeof(uint32_t);
        case LayoutEntryType::Uint32x3:  return 3 * sizeof(uint32_t);
        case LayoutEntryType::Uint32x4:  return 4 * sizeof(uint32_t);

        case LayoutEntryType::Sint32:    return sizeof(int32_t);
        case LayoutEntryType::Sint32x2:  return 2 * sizeof(int32_t);
        case LayoutEntryType::Sint32x3:  return 3 * sizeof(int32_t);
        case LayoutEntryType::Sint32x4:  return 4 * sizeof(int32_t);

        case LayoutEntryType::Unorm10_10_10_2: return 4;  // Packed 10+10+10+2 bits = 4 bytes

        default: return 0;  // Return 0 for unknown formats
    }
}

VertexBufferLayoutData BuildVertexLayout(VertexBufferLayout bufferData)
{
    VertexBufferLayoutData result;
    wgpu::VertexBufferLayout& vertexBufferLayout = result.vertexBufferLayout;
    size_t entriesLength = bufferData.entries.size();
    std::vector<wgpu::VertexAttribute>& attributes = result.attributes;
    attributes.resize(entriesLength);
    size_t currentOffset = 0;

    for (size_t i = 0; i < entriesLength; ++i)
    {
        VertexBufferEntry newEntry;
        newEntry.name = bufferData.entries[i].name;
        newEntry.type = bufferData.entries[i].type;
        newEntry.offsett = currentOffset;

        attributes[i].format = bufferData.entries[i].type;
        attributes[i].shaderLocation = i;
        attributes[i].offset = currentOffset;
        result.offsetMap[bufferData.entries[i].name] = newEntry;
        currentOffset += GetVertexFormatSize(bufferData.entries[i].type);
    }

    vertexBufferLayout.attributeCount = attributes.size();
    vertexBufferLayout.attributes = attributes.data();
    vertexBufferLayout.arrayStride = currentOffset;
    vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;
    return result;
}

wgpu::Buffer CreateVertexBuffer(VertexBufferLayoutData layout, const std::vector<VertexAttribute>& data)
{
    Logging::PrintSuccess("HELLO");
    const uint64_t vertexSize = layout.vertexBufferLayout.arrayStride;
    uint64_t lastNumEntries = 0;
    std::vector<uint8_t> vertexBufferData;
    vertexBufferData.resize(0);
    for (const VertexAttribute& a : data)
    {
        if (!layout.offsetMap.contains(a.name))
        {
            Logging::PrintError("Trying to add a vertex attribute that doesnt exist");
            continue;
        }
        size_t offsett = layout.offsetMap[a.name].offsett;
        size_t entryByteSize = GetVertexFormatSize(layout.offsetMap[a.name].type);

        if(lastNumEntries == 0) lastNumEntries = a.numElements;
        if(lastNumEntries != a.numElements)
        {
            Logging::PrintError("Different amounts of entries for vertex attriubutes?");
            return {};
        }
        if(vertexBufferData.size() == 0)
            vertexBufferData.resize(lastNumEntries * vertexSize, 0);

        const uint8_t* src = static_cast<const uint8_t*>(a.data);

        for (size_t i = 0; i < lastNumEntries; i++) {
            size_t destIndex = i * vertexSize + offsett; 
            std::memcpy(vertexBufferData.data() + destIndex, src + (i * entryByteSize), entryByteSize);
        }
    }

    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = lastNumEntries * vertexSize;
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
    bufferDesc.mappedAtCreation = false;
    wgpu::Buffer vertexBuffer = device.CreateBuffer(&bufferDesc);
    device.GetQueue().WriteBuffer(vertexBuffer, 0, vertexBufferData.data(), bufferDesc.size);
    return vertexBuffer;
}

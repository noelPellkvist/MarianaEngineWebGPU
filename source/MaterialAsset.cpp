#include <MaterialAsset.hpp>
#include <Logger.hpp>

#include <algorithm>
#include <utility>

namespace
{
    struct SlotAllocator
    {
        std::vector<uint32_t> freeSlots;
        uint32_t nextSlot = 0;
    };

    SlotAllocator& MaterialSlots()
    {
        static SlotAllocator slots;
        return slots;
    }
}

MaterialAsset::MaterialAsset()
{
    SlotAllocator& slots = MaterialSlots();
    if (!slots.freeSlots.empty())
    {
        m_GpuSlot = slots.freeSlots.back();
        slots.freeSlots.pop_back();
    }
    else if (slots.nextSlot < kMaxMaterials)
    {
        m_GpuSlot = slots.nextSlot++;
    }
    else
    {
        Logger::Error("Out of GPU material slots; the material will render with the default material");
    }
}

MaterialAsset::~MaterialAsset()
{
    if (m_GpuSlot != kInvalidSlot)
        MaterialSlots().freeSlots.push_back(m_GpuSlot);
}

void MaterialAsset::SetTexture(MaterialTexture slot, AssetHandle<Texture> texture, const SamplerSettings& sampler, uint32_t uvSet)
{
    const uint32_t index = static_cast<uint32_t>(slot);
    if (index >= m_Textures.size())
        return;

    TextureSlot& entry = m_Textures[index];
    entry.texture = std::move(texture);
    entry.sampler = sampler;
    entry.uvSet = uvSet > 0 ? 1u : 0u;

    if (entry.uvSet)
        properties.uvSets |= 1u << index;
    else
        properties.uvSets &= ~(1u << index);
    m_BindGroupDirty = true;
    MarkDirty();
}

void MaterialAsset::SetAlphaMode(AlphaMode mode, float cutoff)
{
    m_AlphaMode = mode;
    // The shaders alpha test whenever the cutoff is above 0.
    properties.alphaCutoff = mode == AlphaMode::Opaque ? 0.0f : std::max(cutoff, 1e-4f);
    MarkDirty();
}

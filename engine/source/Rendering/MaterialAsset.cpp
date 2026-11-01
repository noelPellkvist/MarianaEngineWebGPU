#include <Mariana/Rendering/MaterialAsset.hpp>
#include <Mariana/Foundation/Logger.hpp>

#include <algorithm>
#include <cmath>
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

std::shared_ptr<MaterialAsset> MaterialAsset::Clone() const
{
    auto copy = std::make_shared<MaterialAsset>();
    copy->properties = properties;
    copy->doubleSided = doubleSided;
    copy->m_Textures = m_Textures;
    copy->m_AlphaMode = m_AlphaMode;
    return copy;
}

void MaterialAsset::CopySettingsFrom(const MaterialAsset& other)
{
    if (&other == this)
        return;
    properties = other.properties;
    doubleSided = other.doubleSided;
    m_Textures = other.m_Textures;
    m_AlphaMode = other.m_AlphaMode;
    m_BindGroupDirty = true;
    MarkDirty();
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

void MaterialAsset::SetTextureTransform(MaterialTexture slot, const TextureTransform& transform)
{
    const uint32_t index = static_cast<uint32_t>(slot);
    if (index >= m_Textures.size())
        return;

    m_Textures[index].transform = transform;
    // uv' = T * R * S * uv as KHR_texture_transform has it, R turning counter-clockwise in UV space.
    const float c = std::cos(transform.rotation), s = std::sin(transform.rotation);
    properties.uvMatrix[index] = glm::vec4(c * transform.scale.x, s * transform.scale.y, -s * transform.scale.x, c * transform.scale.y);
    properties.uvOffset[index] = transform.offset;
    MarkDirty();
}

void MaterialAsset::SetAlphaMode(AlphaMode mode, float cutoff)
{
    m_AlphaMode = mode;
    // The shaders alpha test whenever the cutoff is above 0; blended surfaces aren't cut.
    properties.alphaCutoff = mode == AlphaMode::Mask ? std::max(cutoff, 1e-4f) : 0.0f;
    properties.alphaBlend = mode == AlphaMode::Blend ? 1u : 0u;
    MarkDirty();
}

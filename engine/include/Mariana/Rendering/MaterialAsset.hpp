#pragma once
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

#include <Mariana/Graphics/Material.hpp>
#include <Mariana/Graphics/Shader.hpp>
#include <Mariana/Graphics/Texture.hpp>
#include <Mariana/Assets/AssetManager.hpp>

// Per-material constants read by the PBR shaders from their MaterialBuffer. The field order must
// match `MaterialProperties` in test.wgsl, prepass.wgsl and shadow.wgsl.
struct PBRMaterialProperties
{
    glm::vec4 baseColor{1,1,1,1};
    float metallicFactor{0};
    float roughnessFactor{1};

    float normalMapStrength{1};
    float occlusionStrength{1};

    glm::vec3 emissiveFactor{0,0,0};
    float alphaCutoff{0}; // 0 = opaque; set through MaterialAsset::SetAlphaMode

    // Bit i set: texture i (see MaterialTexture) reads TEXCOORD_1; set through MaterialAsset::SetTexture.
    uint32_t uvSets{0};
};

// The textures a PBR material can use, in binding order.
enum class MaterialTexture : uint32_t { BaseColor, Normal, MetallicRoughness, Occlusion, Emissive, Count };

// Blend isn't supported by the renderer yet and draws like Mask.
enum class AlphaMode { Opaque, Mask, Blend };

// A PBR material, independent of where it came from (a glTF file, code, the editor): its constants,
// its textures with their samplers and UV sets, and its render state. Change `properties` and call
// MarkDirty(); change textures through SetTexture. The renderer builds the GPU bind group the first
// time it draws the material and again after a texture changed; empty texture slots get neutral
// defaults (white, or a flat normal). The material's constants occupy a slot in the renderer's
// material storage buffer for the asset's lifetime.
class MaterialAsset
{
public:
    // Must not exceed the element count of StandardPBRPipeline::materialsBuffer.
    static constexpr uint32_t kMaxMaterials = 131072;
    static constexpr uint32_t kInvalidSlot = ~0u;

    struct TextureSlot
    {
        AssetHandle<Texture> texture;
        SamplerSettings sampler;
        uint32_t uvSet = 0; // 0 or 1
    };

    MaterialAsset();
    ~MaterialAsset();

    MaterialAsset(const MaterialAsset&) = delete;
    MaterialAsset& operator=(const MaterialAsset&) = delete;

    PBRMaterialProperties properties;
    // Drawn without back-face culling, back faces lit from their side.
    bool doubleSided = false;

    void SetTexture(MaterialTexture slot, AssetHandle<Texture> texture, const SamplerSettings& sampler = {}, uint32_t uvSet = 0);
    const TextureSlot& GetTexture(MaterialTexture slot) const { return m_Textures[static_cast<size_t>(slot)]; }

    void SetAlphaMode(AlphaMode mode, float cutoff = 0.5f);
    AlphaMode GetAlphaMode() const { return m_AlphaMode; }

    uint32_t GetGpuSlot() const { return m_GpuSlot; }

    // Set when `properties` changed and must be uploaded again (new materials start dirty).
    bool IsDirty() const { return m_Dirty; }
    void MarkDirty() { m_Dirty = true; ++m_Version; }
    void ClearDirty() { m_Dirty = false; }
    // Changes with every edit (MarkDirty, SetTexture, SetAlphaMode).
    uint32_t Version() const { return m_Version; }

    // Renderer side: the texture bind group, rebuilt while BindGroupDirty() (new materials start dirty).
    Material2 material;
    bool BindGroupDirty() const { return m_BindGroupDirty; }
    void ClearBindGroupDirty() { m_BindGroupDirty = false; }

private:
    std::array<TextureSlot, static_cast<size_t>(MaterialTexture::Count)> m_Textures;
    AlphaMode m_AlphaMode = AlphaMode::Opaque;
    uint32_t m_GpuSlot = kInvalidSlot;
    uint32_t m_Version = 0;
    bool m_Dirty = true;
    bool m_BindGroupDirty = true;
};

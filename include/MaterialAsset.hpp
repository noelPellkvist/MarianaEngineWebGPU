#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

#include <Material.hpp>
#include <Texture.hpp>
#include <AssetsSystem/AssetManager.hpp>

// Per-material constants read by the PBR shader from its MaterialBuffer. The field order must
// match `MaterialProperties` in resources/Shaders/test.wgsl.
struct GLTFMaterialProperties
{
    glm::vec4 baseColor{1,1,1,1};
    float metallicFactor{0};
    float roughnessFactor{1};

    float normalMapStrength{1};
    float occlusionStrength{1};

    glm::vec3 emissiveFactor{0,0,0};
    float alphaCutoff{0.5};

    // Bit i set: texture i (albedo, normal, metallic-roughness, occlusion, emissive) uses TEXCOORD_1.
    uint32_t uvSets{0};

};

// A PBR material: its texture bind group, its constants, and the slot those constants occupy in
// the PBR pipeline's material storage buffer. The slot is reserved for the asset's lifetime.
class MaterialAsset
{
public:
    // Must not exceed the element count of StandardPBRPipeline::materialsBuffer.
    static constexpr uint32_t kMaxMaterials = 131072;
    static constexpr uint32_t kInvalidSlot = ~0u;

    MaterialAsset();
    ~MaterialAsset();

    MaterialAsset(const MaterialAsset&) = delete;
    MaterialAsset& operator=(const MaterialAsset&) = delete;

    Material2 material;
    GLTFMaterialProperties properties;
    // Drawn without back-face culling, back faces lit from their side.
    bool doubleSided = false;

    // The textures bound in `material`, held so they count as in use.
    std::vector<AssetHandle<Texture>> textures;

    uint32_t GetGpuSlot() const { return m_GpuSlot; }

    // Set when `properties` changed and must be uploaded again (new materials start dirty).
    bool IsDirty() const { return m_Dirty; }
    void MarkDirty() { m_Dirty = true; }
    void ClearDirty() { m_Dirty = false; }

private:
    uint32_t m_GpuSlot = kInvalidSlot;
    bool m_Dirty = true;
};

#pragma once
#include <vector>
#include <memory>
#include <unordered_map>

#include <Texture.hpp>
#include <Material.hpp>
#include <Mesh.hpp>
#include <Animation.hpp>
#include <Logger.hpp>

enum class AssetType
{
    Unknown,
    Texture,
    Material,
    Mesh,
    Model,
    Animation,
    Shader,
    Font
};

enum class AssetLoadState {
    Discovered,     
    MetadataReady,  
    Queued,
    LoadingCPU,     
    CPUReady,       
    UploadingGPU,
    GPUReady,       
    Failed,
    Unloaded
};

struct AssetHandle
{
    uint64_t id = 0;
};

struct AssetStats {
    uint64_t diskBytes = 0;
    uint64_t cpuBytes = 0;
    uint64_t gpuBytes = 0;
    float progress = 0.0f; // 0..1
};

struct AssetRecord
{
    AssetHandle handle;
    AssetType type = AssetType::Unknown;
    AssetLoadState loadState = AssetLoadState::Discovered;

    std::string filePath;
    std::string name;
    std::string error;

    AssetStats stats;
    uint32_t refCount = 0;

    std::vector<AssetHandle> dependencies;

    int textureIndex = -1;
    int materialIndex = -1;
    int meshIndex = -1;
    int animationIndex = -1;
};

struct GLTFMaterialProperties
{
    glm::vec4 baseColor{1,1,1,1};
    float metallicFactor{0};
    float roughnessFactor{1};

    float normalMapStrength{1};
    float occlusionStrength{1};

    glm::vec3 emissiveFactor{0,0,0};
    float alphaCutoff{0.5};
    
};

struct AnimationTargetEntity
{
    uint32_t nodeIndex;
    uint32_t translationTargetChannel;
    uint32_t rotationTargetChannel;
    uint32_t scaleTargetCHannel;
};

class AssetManager
{
    public:
        static inline std::vector<Texture> LoadedTextures{};
        static inline std::vector<GLTFMaterialProperties> LoadedMaterialProperties{};
        static inline std::vector<Material2> LoadedMaterials{};
        static inline std::vector<Animation> LoadedAnimations{};
        static inline std::vector<std::shared_ptr<IMesh>> LoadedMeshes{};

        AssetManager();
        ~AssetManager();

        size_t GetTextureCount() { return LoadedTextures.size(); }
        size_t GetMaterialCount() { return LoadedMaterials.size(); }
        size_t GetMeshCount() { return LoadedMeshes.size(); }
};
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>
#include <Mariana/Rendering/ICamera.hpp>
#include <Mariana/Geometry/Vertex.hpp>

// Cascaded shadow maps: the camera's view out to the shadow distance is split into kShadowCascades
// slices, each with its own square depth map fitted around it (SceneRendererSettings).
inline constexpr uint32_t kShadowCascades = 4; // the shaders pack per-cascade values into vec4s

struct UBO {
  glm::vec3 lightDir;
  std::array<glm::mat4, kShadowCascades> cascadeVP;
  glm::vec4 cascadeSplits{0.0f}; // view depth where each cascade ends
  glm::vec4 cascadeTexel{0.0f};  // world size of one shadow texel in each cascade
  glm::vec4 sunColor{1.0f};      // rgb = color * intensity
  std::array<glm::vec4, 9> skySH{}; // diffuse sky light, see SkyLight.hpp
  glm::vec4 skyParams{0.0f};     // x = sky diffuse intensity, y = sky reflection intensity, z = sky rotation (radians)
};

// Sun and sky, edited in the Lighting window; EditorApp turns them into the UBO.
struct LightingSettings
{
    float sunAzimuth = 270.0f;  // degrees around +y where the sun stands (0 = +z, 90 = +x)
    float sunElevation = 45.0f; // degrees above the horizon
    glm::vec3 sunColor{ 1.0f };
    float sunIntensity = 1.0f;
    float skyIntensity = 1.0f;        // diffuse light from the sky
    float reflectionIntensity = 1.0f; // the sky reflected by glossy surfaces
    float skyRotation = 0.0f;         // degrees the sky (its light, reflections and background) is turned around +y, like an azimuth

    // The direction the sunlight travels in.
    glm::vec3 LightDirection() const
    {
        const float azimuth = glm::radians(sunAzimuth);
        const float elevation = glm::radians(sunElevation);
        return -glm::vec3(std::cos(elevation) * std::sin(azimuth), std::sin(elevation), std::cos(elevation) * std::cos(azimuth));
    }
};

// Light view-projection of the cascade a shadow pass renders (dynamic uniform, one slot per cascade).
struct ShadowCascadeData {
  glm::mat4 viewProj;
};

struct TransformData {
  glm::mat4x4 modelMatrix;
  glm::mat3x3 normalMatrix;
  glm::uvec2 entityID{0, 0};
  void SetEntityID(uint64_t id) {
    entityID.x = static_cast<uint32_t>(id & 0xFFFFFFFFull);
    entityID.y = static_cast<uint32_t>(id >> 32);
  }
};

// A joint's skinning matrix. Normals are skinned with its upper 3x3 (exact for rotation and
// uniform scale, which is what joints use).
struct BoneData
{
    glm::mat4x4 model = glm::mat4x4(1.0f);
};

struct DrawData
{
    uint32_t transformIndex{0};
    uint32_t materialIndex{0};
    uint32_t boneOffset{~0u}; // first joint matrix in boneBuffer, ~0u = not skinned
};

// The scene's data on the GPU, shared by every view of it (SceneRenderer): the transforms,
// material constants and joint matrices the shaders index.
class StandardPBRPipeline
{
        private:
        TransformData transformData{};
        PBRMaterialProperties materialData{};
        BoneData boneData{};

        StorageArrayLayout transformLayout;
        StorageArrayLayout materialsLayout;
        StorageArrayLayout boneLayout;

        std::vector<std::byte> transformStaging; // CPU copy of the packed transform array
        std::vector<uint32_t> dirtyTransforms;

    public:
        static constexpr uint32_t kMaxTransforms = 131072;
        static constexpr uint32_t kMaxBones = 131072;

        StandardPBRPipeline();
        ~StandardPBRPipeline();

        // Writes the properties of new or changed materials into materialsBuffer.
        void UploadDirtyMaterials(AssetManager2& assets);

        // Stages the transform at slot `index`; FlushTransforms uploads everything staged since
        // the last flush with as few buffer writes as possible.
        void QueueTransform(uint32_t index, const TransformData& data);
        void FlushTransforms();

        Buffer transformBuffer;
        Buffer materialsBuffer;
        Buffer boneBuffer;
    };

// What each view of the scene has of its own (SceneView): the UBO (lighting and its shadow
// cascades), its camera, the per-instance data of its draws and its cascades' matrices.
class StandardPBRView
{
        private:
        UBO ubo{};
        CameraInfo cameraInfo{};
        DrawData drawData{};
        ShadowCascadeData cascadeData{};

        UniformBufferLayout uboLayout;
        UniformBufferLayout camLayout;
        StorageArrayLayout drawLayout;
        UniformBufferLayout cascadeLayout;

    public:
        static constexpr uint32_t kMaxDraws = 131072;

        StandardPBRView();

        Buffer uboBuffer;
        Buffer cameraBuffer;
        Buffer drawBuffer;
        Buffer shadowCascadeBuffer;
    };
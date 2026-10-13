#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

#include <Renderpass.hpp>
#include <Shader.hpp>
#include <Material.hpp>
#include <Buffer.hpp>
#include <Texture.hpp>
#include <UniformLayout.hpp>

class Renderer;

// Per-pass constants of the post-processing chain (dynamic uniform, one slot per pass). The field
// order must match `PostParams` in bloom_down.wgsl, bloom_up.wgsl and composite.wgsl.
struct PostParams
{
    glm::vec4 texel{0.0f};    // xy: 1 / source size, z: filter radius in source texels, w: 1 on the first downsample
    glm::vec4 settings{0.0f}; // first downsample: x threshold, y knee. Composite: x exposure, y bloom intensity, z 1 / level count
};

// Bloom and tonemapping on top of the linear HDR scene color: a 13-tap downsample chain, a tent-filtered
// upsample chain, and a composite pass (bloom mix, exposure, ACES, sRGB encode) that renders to the screen.
class PostProcessPipeline
{
public:
    static constexpr uint32_t kBloomLevels = 6;

    PostProcessPipeline();
    ~PostProcessPipeline();

    // `sceneColor` is the HDR target the scene renders into; `outputFormat` is the swapchain's.
    void Init(uint32_t width, uint32_t height, TextureFormat outputFormat, Texture& sceneColor);
    // Call after the scene target was recreated at the new size.
    void Resize(uint32_t width, uint32_t height);
    // Adds the bloom chain and the composite. Push these after the scene pass so the composite is last.
    void AddPasses(Renderer& renderer);
    // Uploads changed settings; call once per frame before rendering.
    void Update();

    float exposure = 1.5f;
    float bloomIntensity = 1.0f;
    float bloomThreshold = 1.0f; // only HDR light brighter than this blooms
    float bloomKnee = 0.5f;      // softens the threshold over this range
    float bloomRadius = 1.0f;    // upsample filter radius, in texels of each level
    bool bloomEnabled = true;

private:
    static constexpr uint32_t kCompositeSlot = 2 * kBloomLevels - 1;

    std::vector<std::unique_ptr<Renderpass>> downPasses; // level i is 1/2^(i+1) of the screen
    std::vector<std::unique_ptr<Renderpass>> upPasses;   // level i matches downPasses[i]
    std::unique_ptr<Renderpass> compositePass;

    Shader2 downShader{"BloomDownsample"};
    Shader2 upShader{"BloomUpsample"};
    Shader2 compositeShader{"PostComposite"};

    // Built in place and never moved: Material2 keeps pointers into its own resources.
    std::array<Material2, kBloomLevels> downMaterials;
    std::array<Material2, kBloomLevels - 1> upMaterials;
    Material2 compositeMaterial;

    PostParams params{};
    UniformBufferLayout paramsLayout;
    Buffer paramsBuffer;

    Texture* sceneColor = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    PostParams writtenComposite{};
    glm::vec3 writtenLevelSettings{-1.0f}; // radius, threshold, knee

    glm::vec2 LevelSize(uint32_t level) const;
    void WriteLevelParams();
    void BuildMaterials();
};

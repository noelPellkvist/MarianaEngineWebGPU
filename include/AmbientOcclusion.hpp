#pragma once
#include <cstdint>
#include <memory>
#include <glm/glm.hpp>

#include <Renderpass.hpp>
#include <Shader.hpp>
#include <Material.hpp>
#include <Buffer.hpp>
#include <Texture.hpp>
#include <UniformLayout.hpp>

class Renderer;

// Constants of gtao.wgsl and gtao_blur.wgsl (same field order).
struct AOParams
{
    glm::vec4 settings{0.0f}; // x: radius in world units, y: power
    glm::vec4 screen{0.0f};   // xy: size in pixels, zw: 1 / size
};

// Screen-space ambient occlusion with GTAO (Jimenez et al. 2016, structured after Intel's XeGTAO),
// from the prepass depth and normals at full resolution: a noisy horizon search, then a depth-aware
// blur over exactly one tile of its 4x4 noise pattern. The PBR shader applies the result to sky
// light only, with multi-bounce and specular occlusion.
class AmbientOcclusion
{
public:
    AmbientOcclusion();
    ~AmbientOcclusion();

    // `depth` and `normals` are the prepass's (view-space normals packed into RGB10A2).
    void Init(uint32_t width, uint32_t height, Texture& depth, Texture& normals, Buffer& cameraBuffer);
    // Call after the prepass targets were recreated at the new size.
    void Resize(uint32_t width, uint32_t height);
    // Adds the AO and blur passes; push them after the prepass and before the scene pass.
    void AddPasses(Renderer& renderer);
    // Enables the passes and uploads changed settings; call once per frame before rendering.
    void Update();

    // The blurred visibility (1 = unoccluded), screen sized.
    Texture& Result() { return blurPass->GetRenderTarget(0); }

    bool enabled = true;
    float radius = 0.6f; // world units
    float power = 1.5f;  // contrast: higher is darker

private:
    std::unique_ptr<Renderpass> aoPass;
    std::unique_ptr<Renderpass> blurPass;

    Shader2 aoShader{"GTAO"};
    Shader2 blurShader{"GTAOBlur"};

    // Built in place and never moved: Material2 keeps pointers into its own resources.
    Material2 aoMaterial;
    Material2 blurMaterial;

    AOParams params{};
    UniformBufferLayout paramsLayout;
    Buffer paramsBuffer;
    AOParams written{};

    uint32_t width = 0;
    uint32_t height = 0;
};

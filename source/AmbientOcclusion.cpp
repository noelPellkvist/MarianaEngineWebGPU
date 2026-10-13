#include <AmbientOcclusion.hpp>
#include <Renderer.hpp>
#include <FileReader.hpp>

AmbientOcclusion::AmbientOcclusion()
    : paramsLayout(false, params, params.settings, params.screen),
      paramsBuffer(paramsLayout)
{
    paramsBuffer.Build();
}

AmbientOcclusion::~AmbientOcclusion() = default;

void AmbientOcclusion::Init(uint32_t w, uint32_t h, Texture& depth, Texture& normals, Buffer& cameraBuffer)
{
    width = w;
    height = h;

    aoPass = std::make_unique<Renderpass>(false, false, std::vector<TextureFormat>{ TextureFormat::R8Unorm }, width, height);
    aoPass->Init();
    blurPass = std::make_unique<Renderpass>(false, false, std::vector<TextureFormat>{ TextureFormat::R8Unorm }, width, height);
    blurPass->Init();

    // Fullscreen passes: group 0 is the camera and the settings, group 1 (the material group) the inputs.
    aoShader.Group(0)
    .AddBuffer("CameraInfo", 0, cameraBuffer)
    .AddBuffer("AOParams", 1, paramsBuffer);
    aoShader.Group(1)
    .AddTexture("depth", 0, TextureType_Depth)
    .AddTexture("normals", 1, TextureType_2D);
    aoShader.SetMaterialGroup(1);
    aoShader.SetWGSL(FileReader::LoadRawString("/Shaders/gtao.wgsl"));
    aoShader.SetRenderpass(aoPass.get());
    aoShader.Build();

    blurShader.Group(0)
    .AddBuffer("CameraInfo", 0, cameraBuffer)
    .AddBuffer("AOParams", 1, paramsBuffer);
    blurShader.Group(1)
    .AddTexture("depth", 0, TextureType_Depth)
    .AddTexture("ao", 1, TextureType_2D);
    blurShader.SetMaterialGroup(1);
    blurShader.SetWGSL(FileReader::LoadRawString("/Shaders/gtao_blur.wgsl"));
    blurShader.SetRenderpass(blurPass.get());
    blurShader.Build();

    aoMaterial.InitFromShader(aoShader)
        .SetTexture("depth", depth)
        .SetTexture("normals", normals)
        .Build();
    blurMaterial.InitFromShader(blurShader)
        .SetTexture("depth", depth)
        .SetTexture("ao", aoPass->GetRenderTarget(0))
        .Build();

    aoPass->recordCallback = [this]() {
        aoPass->SetShader2(aoShader);
        aoPass->SetMaterial2(aoShader, aoMaterial);
        aoPass->DrawFullscreen();
    };
    blurPass->recordCallback = [this]() {
        blurPass->SetShader2(blurShader);
        blurPass->SetMaterial2(blurShader, blurMaterial);
        blurPass->DrawFullscreen();
    };
}

void AmbientOcclusion::Resize(uint32_t w, uint32_t h)
{
    width = w;
    height = h;
    aoPass->Recreate(width, height);
    blurPass->Recreate(width, height);

    // The materials still point at the same Texture objects, which now hold new GPU textures.
    aoMaterial.Build();
    blurMaterial.Build();
    written = AOParams{};
}

void AmbientOcclusion::AddPasses(Renderer& renderer)
{
    renderer.PushRenderpass(aoPass.get());
    renderer.PushRenderpass(blurPass.get());
}

void AmbientOcclusion::Update()
{
    aoPass->SetEnabled(enabled);
    blurPass->SetEnabled(enabled);

    params.settings = glm::vec4(radius, power, 0.0f, 0.0f);
    params.screen = glm::vec4(static_cast<float>(width), static_cast<float>(height), 1.0f / width, 1.0f / height);
    if (params.settings != written.settings || params.screen != written.screen)
    {
        paramsBuffer.Write(params, 0);
        written = params;
    }
}

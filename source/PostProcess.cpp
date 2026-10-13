#include <PostProcess.hpp>
#include <Renderer.hpp>
#include <FileReader.hpp>

#include <algorithm>

PostProcessPipeline::PostProcessPipeline()
    : paramsLayout(true, params, params.texel, params.settings),
      paramsBuffer(paramsLayout)
{
    paramsBuffer.Build();
}

PostProcessPipeline::~PostProcessPipeline() = default;

glm::vec2 PostProcessPipeline::LevelSize(uint32_t level) const
{
    return glm::vec2(std::max(1u, width >> (level + 1)), std::max(1u, height >> (level + 1)));
}

void PostProcessPipeline::Init(uint32_t w, uint32_t h, TextureFormat outputFormat, Texture& scene)
{
    width = w;
    height = h;
    sceneColor = &scene;

    for (uint32_t i = 0; i < kBloomLevels; ++i)
    {
        const glm::vec2 size = LevelSize(i);
        downPasses.push_back(std::make_unique<Renderpass>(false, false, std::vector<TextureFormat>{ TextureFormat::RGBA16Float }, (uint32_t)size.x, (uint32_t)size.y));
        downPasses.back()->Init();
        if (i + 1 < kBloomLevels)
        {
            upPasses.push_back(std::make_unique<Renderpass>(false, false, std::vector<TextureFormat>{ TextureFormat::RGBA16Float }, (uint32_t)size.x, (uint32_t)size.y));
            upPasses.back()->Init();
        }
    }
    compositePass = std::make_unique<Renderpass>(false, false, std::vector<TextureFormat>{ outputFormat }, width, height);
    compositePass->Init();

    // All three are fullscreen passes: no vertex layout, no depth. Group 0 is shared constants,
    // group 1 (the material group) holds each pass's input textures.
    downShader.Group(0)
    .AddBuffer("PostParams", 0, paramsBuffer)
    .AddSampler("linearSampler", 1, SamplerType_LinearClamp);
    downShader.Group(1)
    .AddTexture("source", 0, TextureType_2D);
    downShader.SetMaterialGroup(1);
    downShader.SetWGSL(FileReader::LoadRawString("/Shaders/bloom_down.wgsl"));
    downShader.SetRenderpass(downPasses[0].get());
    downShader.Build();

    upShader.Group(0)
    .AddBuffer("PostParams", 0, paramsBuffer)
    .AddSampler("linearSampler", 1, SamplerType_LinearClamp);
    upShader.Group(1)
    .AddTexture("source", 0, TextureType_2D)
    .AddTexture("current", 1, TextureType_2D);
    upShader.SetMaterialGroup(1);
    upShader.SetWGSL(FileReader::LoadRawString("/Shaders/bloom_up.wgsl"));
    upShader.SetRenderpass(upPasses[0].get());
    upShader.Build();

    compositeShader.Group(0)
    .AddBuffer("PostParams", 0, paramsBuffer)
    .AddSampler("linearSampler", 1, SamplerType_LinearClamp);
    compositeShader.Group(1)
    .AddTexture("sceneColor", 0, TextureType_2D)
    .AddTexture("bloomColor", 1, TextureType_2D);
    compositeShader.SetMaterialGroup(1);
    compositeShader.SetWGSL(FileReader::LoadRawString("/Shaders/composite.wgsl"));
    compositeShader.SetRenderpass(compositePass.get());
    compositeShader.Build();

    // Each level reads the previous one: down 0 reads the scene, up i reads the smaller level
    // (the last downsample, or the upsample below it) plus its own downsample.
    for (uint32_t i = 0; i < kBloomLevels; ++i)
    {
        downMaterials[i].InitFromShader(downShader)
            .SetTexture("source", i == 0 ? *sceneColor : downPasses[i - 1]->GetRenderTarget(0));
    }
    for (uint32_t i = 0; i + 1 < kBloomLevels; ++i)
    {
        Texture& smaller = (i + 2 == kBloomLevels) ? downPasses[i + 1]->GetRenderTarget(0) : upPasses[i + 1]->GetRenderTarget(0);
        upMaterials[i].InitFromShader(upShader)
            .SetTexture("source", smaller)
            .SetTexture("current", downPasses[i]->GetRenderTarget(0));
    }
    compositeMaterial.InitFromShader(compositeShader)
        .SetTexture("sceneColor", *sceneColor)
        .SetTexture("bloomColor", upPasses[0]->GetRenderTarget(0));
    BuildMaterials();
    WriteLevelParams();

    for (uint32_t i = 0; i < kBloomLevels; ++i)
    {
        downPasses[i]->recordCallback = [this, i]() {
            Renderpass& pass = *downPasses[i];
            pass.SetShader2(downShader);
            pass.SetBufferIndex("PostParams", i);
            pass.SetMaterial2(downShader, downMaterials[i]);
            pass.DrawFullscreen();
        };
    }
    for (uint32_t i = 0; i + 1 < kBloomLevels; ++i)
    {
        upPasses[i]->recordCallback = [this, i]() {
            Renderpass& pass = *upPasses[i];
            pass.SetShader2(upShader);
            pass.SetBufferIndex("PostParams", kBloomLevels + i);
            pass.SetMaterial2(upShader, upMaterials[i]);
            pass.DrawFullscreen();
        };
    }
    compositePass->recordCallback = [this]() {
        Renderpass& pass = *compositePass;
        pass.SetShader2(compositeShader);
        pass.SetBufferIndex("PostParams", kCompositeSlot);
        pass.SetMaterial2(compositeShader, compositeMaterial);
        pass.DrawFullscreen();
    };
}

void PostProcessPipeline::Resize(uint32_t w, uint32_t h)
{
    width = w;
    height = h;

    for (uint32_t i = 0; i < kBloomLevels; ++i)
    {
        const glm::vec2 size = LevelSize(i);
        downPasses[i]->Recreate((uint32_t)size.x, (uint32_t)size.y);
        if (i + 1 < kBloomLevels)
            upPasses[i]->Recreate((uint32_t)size.x, (uint32_t)size.y);
    }
    compositePass->Recreate(width, height);

    // The materials still point at the same Texture objects, which now hold new GPU textures.
    BuildMaterials();
    WriteLevelParams();
    writtenComposite = PostParams{};
}

void PostProcessPipeline::AddPasses(Renderer& renderer)
{
    for (auto& pass : downPasses)
        renderer.PushRenderpass(pass.get());
    for (uint32_t i = kBloomLevels - 1; i-- > 0;)
        renderer.PushRenderpass(upPasses[i].get());
    renderer.PushRenderpass(compositePass.get());
}

void PostProcessPipeline::BuildMaterials()
{
    for (Material2& material : downMaterials)
        material.Build();
    for (Material2& material : upMaterials)
        material.Build();
    compositeMaterial.Build();
}

void PostProcessPipeline::WriteLevelParams()
{
    for (uint32_t i = 0; i < kBloomLevels; ++i)
    {
        const glm::vec2 source = i == 0 ? glm::vec2(width, height) : LevelSize(i - 1);
        const glm::vec4 settings = i == 0 ? glm::vec4(bloomThreshold, bloomKnee, 0.0f, 0.0f) : glm::vec4(0.0f);
        paramsBuffer.Write(PostParams{ glm::vec4(1.0f / source, 0.0f, i == 0 ? 1.0f : 0.0f), settings }, i);
    }
    for (uint32_t i = 0; i + 1 < kBloomLevels; ++i)
    {
        const glm::vec2 source = LevelSize(i + 1);
        paramsBuffer.Write(PostParams{ glm::vec4(1.0f / source, bloomRadius, 0.0f), glm::vec4(0.0f) }, kBloomLevels + i);
    }
    writtenLevelSettings = glm::vec3(bloomRadius, bloomThreshold, bloomKnee);
}

void PostProcessPipeline::Update()
{
    for (auto& pass : downPasses)
        pass->SetEnabled(bloomEnabled);
    for (auto& pass : upPasses)
        pass->SetEnabled(bloomEnabled);

    if (glm::vec3(bloomRadius, bloomThreshold, bloomKnee) != writtenLevelSettings)
        WriteLevelParams();

    const PostParams composite{ glm::vec4(1.0f / LevelSize(0), bloomRadius, 0.0f),
                                glm::vec4(exposure, bloomEnabled ? bloomIntensity : 0.0f, 1.0f / kBloomLevels, 0.0f) };
    if (composite.texel != writtenComposite.texel || composite.settings != writtenComposite.settings)
    {
        paramsBuffer.Write(composite, kCompositeSlot);
        writtenComposite = composite;
    }
}

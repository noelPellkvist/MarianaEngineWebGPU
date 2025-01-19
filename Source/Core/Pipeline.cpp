#include "Pipeline.hpp"
#include "Resources.h"
#include "GlobalVaribles.hpp"

#include <string>
#include <iostream>

Pipeline::Pipeline(const char* shaderName, wgpu::TextureFormat format, wgpu::Buffer* ubo, wgpu::Sampler* sampler, const std::vector<BindingType>& bindings) : 
    UboBuffer(ubo), sampler(sampler), format(format), bindings(bindings)
{
    using namespace wgpu;
    wgpu::ShaderModule shaderModule = Resources::LoadShader("/Shaders/" +  std::string(shaderName));

    if (shaderModule == nullptr) {
      std::cerr << "Could not load shader!" << std::endl;
      exit(1);
    } else std::cout << "Loaded shader succesfully" << std::endl;

    ColorTargetState colorTargetState{.format = format};

    FragmentState fragmentState{.module = shaderModule,
                                    .targetCount = 1,
                                    .targets = &colorTargetState};

    CreatePipelineLayout();
    InitDepthTexture();

    DepthStencilState depthStencilState = {};
    depthStencilState.depthCompare = CompareFunction::LessEqual;
    depthStencilState.depthWriteEnabled = true;
    depthStencilState.format = TextureFormat::Depth24Plus;
    depthStencilState.stencilReadMask = 0;
    depthStencilState.stencilWriteMask = 0;

    RenderPipelineDescriptor descriptor{
      .layout = layout,
      .vertex = {.module = shaderModule,
                 .bufferCount = 2,
                 .buffers = vertexBufferLayouts.data()},
      .depthStencil = &depthStencilState,
      .fragment = &fragmentState};
    pipeline = device.CreateRenderPipeline(&descriptor);
}

void Pipeline::CreatePipelineLayout()
{
    using namespace wgpu;
    PopulateVertexBufferLayouts();
    bindgroupLayouts.reserve(bindings.size());
    for (int i = 0; i < bindings.size(); i++)
    {
        switch (bindings[i])
        {
          case BindingType::BUILT_IN_UBO:
            PopulateGlobalBindings();
            break;
          case BindingType::BUILT_IN_MODELDATA:
            PopulateModelBindings();
            break;
          case BindingType::BUILT_IN_BONES:
            PopulateBoneBindings();
            break;
          case BindingType::e2D:
            BindExtraTexture(BindingType::e2D);
            break;
          case BindingType::eCube:
            BindExtraTexture(BindingType::eCube);
            break;
          default: 
            std::cout << "BindingType not yet supported (WIP)" << std::endl;
            break;
        }
    }
    PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = bindgroupLayouts.size();
    layoutDesc.bindGroupLayouts = bindgroupLayouts.data();
    layout = device.CreatePipelineLayout(&layoutDesc);

}

void Pipeline::BindExtraTexture(BindingType type)
{
    using namespace wgpu;
    textureBindingLayouts.resize(1);
    textureBindingLayouts[0] = {};
    textureBindingLayouts[0].binding = 0;
    textureBindingLayouts[0].visibility = ShaderStage::Fragment;
    textureBindingLayouts[0].texture.sampleType = TextureSampleType::Float;
    switch (type)
    {
      case (BindingType::eCube):
        textureBindingLayouts[0].texture.viewDimension = TextureViewDimension::Cube;
      break;
      case (BindingType::e2D):
        textureBindingLayouts[0].texture.viewDimension = TextureViewDimension::e2D;
      break;
      default:
        textureBindingLayouts[0].texture.viewDimension = TextureViewDimension::e2D;
      break;
    }

    BindGroupLayoutDescriptor textureBindGroupLayoutDesc{};
    textureBindGroupLayoutDesc.entryCount = (uint32_t)textureBindingLayouts.size();
    textureBindGroupLayoutDesc.entries = textureBindingLayouts.data();
    BindGroupLayout textureBindGroupLayout = device.CreateBindGroupLayout(&textureBindGroupLayoutDesc);

    bindgroupLayouts.push_back(textureBindGroupLayout);
}

void Pipeline::PopulateGlobalBindings()
{
    using namespace wgpu;
    std::vector<BindGroupLayoutEntry> globalBindingLayouts(2);
    globalBindingLayouts[0] = {};
    globalBindingLayouts[0].binding = 0;
    globalBindingLayouts[0].visibility = ShaderStage::Vertex | ShaderStage::Fragment;
    globalBindingLayouts[0].buffer.type = BufferBindingType::Uniform;
    globalBindingLayouts[0].buffer.minBindingSize = sizeof(UBO);

    globalBindingLayouts[1] = {};
    globalBindingLayouts[1].binding = 1;
    globalBindingLayouts[1].visibility = ShaderStage::Fragment;
    globalBindingLayouts[1].sampler.type = SamplerBindingType::Filtering;

    BindGroupLayoutDescriptor bindGroupLayoutDesc1{};
    bindGroupLayoutDesc1.entryCount = (uint32_t)globalBindingLayouts.size();
    bindGroupLayoutDesc1.entries = globalBindingLayouts.data();
    wgpu::BindGroupLayout bindGroupLayout1 = device.CreateBindGroupLayout(&bindGroupLayoutDesc1);

    std::vector<BindGroupEntry> bindings(2);

    bindings[0] = {};
    bindings[0].binding = 0;
    bindings[0].buffer = *UboBuffer;
    bindings[0].offset = 0;
    bindings[0].size = sizeof(UBO);

    bindings[1] = {};
    bindings[1].binding = 1;
    bindings[1].sampler = *sampler;

    BindGroupDescriptor bindGroupDesc{};
    bindGroupDesc.layout = bindGroupLayout1;
    bindGroupDesc.entryCount = (uint32_t)bindings.size();
    bindGroupDesc.entries = bindings.data();
    uboBindGroup = device.CreateBindGroup(&bindGroupDesc);

    bindgroupLayouts.push_back(bindGroupLayout1);
}

void Pipeline::PopulateModelBindings()
{
    using namespace wgpu;
    std::vector<BindGroupLayoutEntry> modelBindingLayouts(1);
    modelBindingLayouts[0] = {};
    modelBindingLayouts[0].binding = 0;
    modelBindingLayouts[0].visibility = ShaderStage::Vertex | ShaderStage::Fragment;
    modelBindingLayouts[0].buffer.type = BufferBindingType::Uniform;
    modelBindingLayouts[0].buffer.hasDynamicOffset = true;
    modelBindingLayouts[0].buffer.minBindingSize = sizeof(ModelData);

    BindGroupLayoutDescriptor modelBindGroupLayoutDesc{};
    modelBindGroupLayoutDesc.entryCount = (uint32_t)modelBindingLayouts.size();
    modelBindGroupLayoutDesc.entries = modelBindingLayouts.data();
    wgpu::BindGroupLayout modelBindGroupLayout = device.CreateBindGroupLayout(&modelBindGroupLayoutDesc);

    bindgroupLayouts.push_back(modelBindGroupLayout);
}

void Pipeline::PopulateBoneBindings()
{
    using namespace wgpu;
    std::vector<wgpu::BindGroupLayoutEntry> boneBindingLayouts(1);
    boneBindingLayouts[0] = {};
    boneBindingLayouts[0].binding = 0;
    boneBindingLayouts[0].visibility = wgpu::ShaderStage::Vertex;
    boneBindingLayouts[0].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    boneBindingLayouts[0].buffer.hasDynamicOffset = false; 
    boneBindingLayouts[0].buffer.minBindingSize = 0; 
    
    wgpu::BindGroupLayoutDescriptor boneBindGroupLayoutDesc{};
    boneBindGroupLayoutDesc.entryCount = (uint32_t)boneBindingLayouts.size();
    boneBindGroupLayoutDesc.entries = boneBindingLayouts.data();
    wgpu::BindGroupLayout boneBindGroupLayout = device.CreateBindGroupLayout(&boneBindGroupLayoutDesc);
    bindgroupLayouts.push_back(boneBindGroupLayout);
}

void Pipeline::PopulateVertexBufferLayouts()
{
    using namespace wgpu;
    VertexBufferLayout vertexBufferLayout;
    attributes.resize(4);

    attributes[0].format = VertexFormat::Float32x3;
    attributes[0].offset = 0;
    attributes[0].shaderLocation = 0;

    attributes[1].format = VertexFormat::Float32x3;
    attributes[1].offset = sizeof(glm::vec3);
    attributes[1].shaderLocation = 1;

    attributes[2].format = VertexFormat::Float32x3;
    attributes[2].offset = 2 * sizeof(glm::vec3);
    attributes[2].shaderLocation = 2;

    attributes[3].format = VertexFormat::Float32x2;
    attributes[3].offset = 3 * sizeof(glm::vec3);
    attributes[3].shaderLocation = 3;

    vertexBufferLayout.attributeCount = attributes.size();
    vertexBufferLayout.attributes = attributes.data();
    vertexBufferLayout.arrayStride = sizeof(Vertex);
    vertexBufferLayout.stepMode = VertexStepMode::Vertex;

    VertexBufferLayout skinnedVertexBufferLayout;
    skinnedVertexAttributes.resize(2);

    skinnedVertexAttributes[0].format = VertexFormat::Sint32x4;
    skinnedVertexAttributes[0].offset = 0;
    skinnedVertexAttributes[0].shaderLocation = 4;

    skinnedVertexAttributes[1].format = VertexFormat::Float32x4;
    skinnedVertexAttributes[1].offset = sizeof(glm::ivec4);
    skinnedVertexAttributes[1].shaderLocation = 5;

    skinnedVertexBufferLayout.attributeCount = skinnedVertexAttributes.size();
    skinnedVertexBufferLayout.attributes = skinnedVertexAttributes.data();
    skinnedVertexBufferLayout.arrayStride = sizeof(SkinnedVertex);
    skinnedVertexBufferLayout.stepMode = VertexStepMode::Vertex;

    vertexBufferLayouts = { vertexBufferLayout, skinnedVertexBufferLayout };
}   

void Pipeline::InitDepthTexture()
{
    using namespace wgpu;
    if (depthTextureView)
    {
      depthTextureView = nullptr;
    }

    TextureFormat depthTextureFormat = TextureFormat::Depth24Plus;
    TextureDescriptor depthTextureDesc;
    depthTextureDesc.dimension = TextureDimension::e2D;
    depthTextureDesc.format = depthTextureFormat;
    depthTextureDesc.mipLevelCount = 1;
    depthTextureDesc.sampleCount = 1;
    depthTextureDesc.size = {(uint32_t)kWidth, (uint32_t)kHeight, 1};
    depthTextureDesc.usage = TextureUsage::RenderAttachment;
    depthTextureDesc.viewFormatCount = 1;
    depthTextureDesc.viewFormats = &depthTextureFormat;
    Texture depthTexture = device.CreateTexture(&depthTextureDesc);

    TextureViewDescriptor depthTextureViewDesc;
    depthTextureViewDesc.aspect = TextureAspect::DepthOnly;
    depthTextureViewDesc.baseArrayLayer = 0;
    depthTextureViewDesc.arrayLayerCount = 1;
    depthTextureViewDesc.baseMipLevel = 0;
    depthTextureViewDesc.mipLevelCount = 1;
    depthTextureViewDesc.dimension = TextureViewDimension::e2D;
    depthTextureViewDesc.format = depthTextureFormat;
    depthTextureView = depthTexture.CreateView(&depthTextureViewDesc);
}

Pipeline::~Pipeline()
{
    
}
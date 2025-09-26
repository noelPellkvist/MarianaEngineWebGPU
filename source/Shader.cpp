#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> 
#include <glm/ext/matrix_clip_space.hpp>

#include <Shader.hpp>
#include <Logger.hpp>
#include <Init.hpp>
#include <VertexBufferLayout.hpp>
#include <Mesh.hpp>
#include <UniformLayout.hpp>
#include <moved_later/GLTFLoader.hpp>
#include <ICamera.hpp>

struct UBO {
  glm::vec3 lightDir;
};

struct TransformData {
  glm::mat4x4 modelMatrix;
  glm::mat4x4 normalMatrix;
};

UBO ubo{};
UniformLayout uboLayout(false, ubo, ubo.lightDir);

TransformData modelsBuffer{};
UniformLayout modelsLayout(true, modelsBuffer, modelsBuffer.modelMatrix, modelsBuffer.normalMatrix);

Shader::Shader(uint8_t textureCount) : NumberOfTextures(textureCount)
{
}

void Shader::WriteToModel(glm::mat4 model)
{
  modelsBuffer.modelMatrix = model;
  modelsBuffer.normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelsBuffer.modelMatrix)));
  modelsLayout.pack(modelsBuffer, 0);
}

void Shader::WriteToUBO(glm::mat4 view, glm::vec3 cameraPos, float aspect)
{
    using clock = std::chrono::steady_clock;
    static const auto t0 = clock::now();
    const float t = std::chrono::duration<float>(clock::now() - t0).count();

    const float degPerSec = 45.0f;            
    const float angle = glm::radians(degPerSec) * t;
    ubo.lightDir = glm::normalize(glm::vec3(1.0f, 0.5f, -1.0f));

    uboLayout.pack(ubo);
    static bool init = false;
    if(init) return;
    modelsBuffer.modelMatrix = glm::translate(glm::mat4(1.0f), {0.0f, 0.0f, 0.0f});
    modelsBuffer.modelMatrix = glm::scale(modelsBuffer.modelMatrix, { 5.0f, 5.0f, 5.0f});
    modelsBuffer.modelMatrix = glm::rotate(modelsBuffer.modelMatrix, angle, glm::vec3(0.0f, 1.0f, 0.0f));
    modelsBuffer.normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelsBuffer.modelMatrix)));
    modelsLayout.pack(modelsBuffer, 0);
    init = true;
}


Shader::~Shader()
{

}

const wgpu::BindGroup& Shader::GetBindGroup() const
{
  return uboLayout.GetBindGroup();
}

const wgpu::BindGroup& Shader::GetModelBindGroup() const
{
  return modelsLayout.GetBindGroup();
}

void Shader::FixTextureBindings()
{
  std::vector<wgpu::BindGroupLayoutEntry> entries;
  entries.resize(NumberOfTextures + 1);
  for(size_t i = 0; i < NumberOfTextures; i++)
  {
    entries[i] = {};
    entries[i].binding = i;
    entries[i].visibility = wgpu::ShaderStage::Fragment;
    entries[i].texture.sampleType = wgpu::TextureSampleType::Float;
    entries[i].texture.viewDimension = wgpu::TextureViewDimension::e2D;
  }

  entries[NumberOfTextures] = {};
  entries[NumberOfTextures].binding = 5;
  entries[NumberOfTextures].visibility = wgpu::ShaderStage::Fragment;
  entries[NumberOfTextures].sampler.type = wgpu::SamplerBindingType::Filtering;

  wgpu::BindGroupLayoutDescriptor textureBindingLayout{};
  textureBindingLayout.entryCount = entries.size();
  textureBindingLayout.entries = entries.data();
  textureBindgroupLayout = device.CreateBindGroupLayout(&textureBindingLayout);
}

void Shader::LoadShader(std::string shaderCode, std::vector<wgpu::TextureFormat> outputFormats)
{
    FixTextureBindings();
    uboLayout.Init();
    modelsLayout.Init();

    CameraInfo c;
    UniformLayout<CameraInfo> camBuf(false,
                      c,
                      c.proj, 
                      c.view, 
                      c.viewProj, 
                      c.invView, 
                      c.invProj, 
                      c.invViewProj, 
                      c.pos, 
                      c.exposure);
    camBuf.Init();

    GLTF::Vertex v{};
    VertexBufferLayout vertexLayout{v, v.position, v.normal, v.tangent, v.texcoord0, v.texcoord1, v.color0};

    

    wgpu::ShaderSourceWGSL wgsl{{.code = shaderCode.c_str()}};
    wgpu::ShaderModuleDescriptor shaderModuleDescriptor{.nextInChain = &wgsl};

    wgpu::ShaderModule shaderModule =
    device.CreateShaderModule(&shaderModuleDescriptor);

    wgpu::ColorTargetState colorTargetState{.format = outputFormats[0]};

    wgpu::FragmentState fragmentState{
      .module = shaderModule, .targetCount = 1, .targets = &colorTargetState};

    wgpu::DepthStencilState depthStencilState{};
    depthStencilState.depthCompare = wgpu::CompareFunction::Less;
    depthStencilState.depthWriteEnabled = true;
    depthStencilState.format = wgpu::TextureFormat::Depth24Plus;
    depthStencilState.stencilReadMask = 0;
    depthStencilState.stencilWriteMask = 0;

    std::vector<wgpu::BindGroupLayout> bindGroupLayouts = {uboLayout.GetBindGroupLayout(), modelsLayout.GetBindGroupLayout(), textureBindgroupLayout, camBuf.GetBindGroupLayout()};

    wgpu::PipelineLayoutDescriptor  layoutDesc = {};
    layoutDesc.bindGroupLayoutCount = bindGroupLayouts.size();
    layoutDesc.bindGroupLayouts = bindGroupLayouts.data();
    m_Layout = device.CreatePipelineLayout(&layoutDesc);


    wgpu::RenderPipelineDescriptor descriptor{  .layout = m_Layout,
                                                .vertex = {
                                                  .module = shaderModule,
                                                  .bufferCount = 1,
                                                  .buffers = &vertexLayout.vertexBufferLayout
                                                },
                                                .primitive = {
                                                  .stripIndexFormat = wgpu::IndexFormat::Undefined,
                                                  .frontFace = wgpu::FrontFace::CW,
                                                  .cullMode = wgpu::CullMode::Back
                                                },
                                             .depthStencil = &depthStencilState,
                                             .multisample = {
                                                .count = 4,
                                                .mask = ~0u,
                                                .alphaToCoverageEnabled = false
                                             },
                                             .fragment = &fragmentState};

    m_Pipeline = device.CreateRenderPipeline(&descriptor);
}

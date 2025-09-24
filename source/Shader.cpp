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
  glm::mat4x4 projection;
  glm::mat4x4 view;
  glm::vec3 lightDir;
  glm::vec3 cameraPos;
};

struct TransformData {
  glm::mat4x4 modelMatrix;
  glm::mat4x4 normalMatrix;
};

UBO ubo{};
UniformLayout uboLayout(false, ubo, ubo.projection, ubo.view, ubo.lightDir, ubo.cameraPos);

TransformData modelsBuffer{};
UniformLayout modelsLayout(true, modelsBuffer, modelsBuffer.modelMatrix, modelsBuffer.normalMatrix);

Shader::Shader(uint8_t textureCount) : NumberOfTextures(textureCount)
{
}

void Shader::WriteToUBO(glm::mat4 view, glm::vec3 cameraPos, float aspect)
{
    using clock = std::chrono::steady_clock;
    static const auto t0 = clock::now();
    const float t = std::chrono::duration<float>(clock::now() - t0).count();

    // --- camera & projection
    const float fovDeg = 60.0f;
    const float zNear  = 0.1f;
    const float zFar   = 100.0f;
    ubo.projection = glm::perspectiveLH_ZO(glm::radians(fovDeg), aspect, zNear, zFar);

    // const glm::vec3 eye{0.0f, 0.0f, 0.0f};
    // const glm::vec3 target{0.0f, 0.0f, 1.0f};
    // const glm::vec3 up{0.0f, 1.0f, 0.0f};
    //ubo.view = glm::lookAtLH(eye, target, up);
    ubo.view = view;

    // --- model: spin around +Y
    const float degPerSec = 45.0f;                 // rotation speed
    const float angle = glm::radians(degPerSec) * t;

    

    // --- light: fixed direction
    ubo.lightDir = glm::normalize(glm::vec3(1.0f, 0.5f, -1.0f));

    ubo.cameraPos = cameraPos;

    // (In your shader you were doing L = normalize(-lightDir); keep that convention.)

    uboLayout.pack(ubo);
    modelsBuffer.modelMatrix = glm::translate(glm::mat4(1.0f), {0.0f, 0.0f, 0.0f});
    modelsBuffer.modelMatrix = glm::scale(modelsBuffer.modelMatrix, { 0.4f, 0.4f, 0.4f});
    modelsBuffer.modelMatrix = glm::rotate(modelsBuffer.modelMatrix, angle, glm::vec3(0.0f, 1.0f, 0.0f));
    modelsBuffer.normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelsBuffer.modelMatrix)));
    modelsLayout.pack(modelsBuffer, 0);
     

    // modelsBuffer.modelMatrix = glm::translate(glm::mat4(1.0f), {0.0,0.0,0.0});
    // modelsBuffer.modelMatrix = glm::rotate(modelsBuffer.modelMatrix, angle, glm::vec3(0.0f, 1.0f, 0.0f));
    // modelsBuffer.normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelsBuffer.modelMatrix)));
    // modelsLayout.pack(modelsBuffer, 0);

    // modelsBuffer.modelMatrix = glm::translate(glm::mat4(1.0f), {0,0,-3});
    // modelsBuffer.normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelsBuffer.modelMatrix)));
    // modelsLayout.pack(modelsBuffer, 1);
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

void Shader::FixTextureBindings(uint8_t NumberOfTextures)
{
  textureBinding = {};
  textureBinding.binding = 0;
  textureBinding.visibility = wgpu::ShaderStage::Fragment;
  textureBinding.texture.sampleType = wgpu::TextureSampleType::Float;
  textureBinding.texture.viewDimension = wgpu::TextureViewDimension::e2D;

  textureBinding2 = {};
  textureBinding2.binding = 1;
  textureBinding2.visibility = wgpu::ShaderStage::Fragment;
  textureBinding2.texture.sampleType = wgpu::TextureSampleType::Float;
  textureBinding2.texture.viewDimension = wgpu::TextureViewDimension::e2D;

  textureBinding3 = {};
  textureBinding3.binding = 2;
  textureBinding3.visibility = wgpu::ShaderStage::Fragment;
  textureBinding3.texture.sampleType = wgpu::TextureSampleType::Float;
  textureBinding3.texture.viewDimension = wgpu::TextureViewDimension::e2D;

  textureBinding4 = {};
  textureBinding4.binding = 3;
  textureBinding4.visibility = wgpu::ShaderStage::Fragment;
  textureBinding4.texture.sampleType = wgpu::TextureSampleType::Float;
  textureBinding4.texture.viewDimension = wgpu::TextureViewDimension::e2D;

  textureBinding5 = {};
  textureBinding5.binding = 4;
  textureBinding5.visibility = wgpu::ShaderStage::Fragment;
  textureBinding5.texture.sampleType = wgpu::TextureSampleType::Float;
  textureBinding5.texture.viewDimension = wgpu::TextureViewDimension::e2D;

  samplerBinding = {};
  samplerBinding.binding = 5;
  samplerBinding.visibility = wgpu::ShaderStage::Fragment;
  samplerBinding.sampler.type = wgpu::SamplerBindingType::Filtering;

  std::vector<wgpu::BindGroupLayoutEntry> entries = {textureBinding, textureBinding2, textureBinding3, textureBinding4, textureBinding5, samplerBinding};

  wgpu::BindGroupLayoutDescriptor textureBindingLayout{};
  textureBindingLayout.entryCount = entries.size();
  textureBindingLayout.entries = entries.data();
  textureBindgroupLayout = device.CreateBindGroupLayout(&textureBindingLayout);
}

void Shader::LoadShader(std::string shaderCode, std::vector<wgpu::TextureFormat> outputFormats)
{
    FixTextureBindings(NumberOfTextures);
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

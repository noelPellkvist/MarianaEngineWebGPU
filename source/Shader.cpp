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

struct UBO {
  glm::mat4x4 projection;
  glm::mat4x4 view;
  glm::mat4x4 model;
  glm::mat4x4 normalMatrix;
  glm::vec3 lightDir;
  glm::vec3 cameraPos;
};

UBO ubo{};
UniformLayout uboLayout(false, ubo, ubo.projection, ubo.view, ubo.model, ubo.normalMatrix, ubo.lightDir, ubo.cameraPos);

Shader::Shader(uint8_t textureCount) : NumberOfTextures(textureCount)
{
}

void Shader::WriteToUBO(glm::mat4 view, glm::vec3 cameraPos)
{
    using clock = std::chrono::steady_clock;
    static const auto t0 = clock::now();
    const float t = std::chrono::duration<float>(clock::now() - t0).count();

    // --- camera & projection
    const float fovDeg = 60.0f;
    const float aspect = 16.0f / 9.0f;
    const float zNear  = 0.1f;
    const float zFar   = 100.0f;
    ubo.projection = glm::perspectiveLH_ZO(glm::radians(fovDeg), aspect, zNear, zFar);

    // const glm::vec3 eye{0.0f, 0.0f, 0.0f};
    // const glm::vec3 target{0.0f, 0.0f, 1.0f};
    // const glm::vec3 up{0.0f, 1.0f, 0.0f};
    //ubo.view = glm::lookAtLH(eye, target, up);
    ubo.view = view;

    // --- model: spin around +Y
    const glm::vec3 pos{0.0f, -0.75f, 3.0f};
    const float degPerSec = 45.0f;                 // rotation speed
    const float angle = glm::radians(degPerSec) * t;

    ubo.model = glm::translate(glm::mat4(1.0f), pos);
    ubo.model = glm::rotate(ubo.model, angle, glm::vec3(0.0f, 1.0f, 0.0f));

    // Normal matrix from model (top-left 3x3 inverse-transpose)
    ubo.normalMatrix = glm::transpose(glm::inverse(glm::mat3(ubo.model)));

    // --- light: fixed direction
    ubo.lightDir = glm::normalize(glm::vec3(1.0f, 0.5f, -1.0f));

    ubo.cameraPos = cameraPos;

    // (In your shader you were doing L = normalize(-lightDir); keep that convention.)

    uboLayout.pack(ubo);
}


Shader::~Shader()
{

}

wgpu::BindGroup& Shader::GetBindGroup()
{
  return uboLayout.GetBindGroup();
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

  samplerBinding = {};
  samplerBinding.binding = 4;
  samplerBinding.visibility = wgpu::ShaderStage::Fragment;
  samplerBinding.sampler.type = wgpu::SamplerBindingType::Filtering;

  std::vector<wgpu::BindGroupLayoutEntry> entries = {textureBinding, textureBinding2, textureBinding3, textureBinding4, samplerBinding};

  wgpu::BindGroupLayoutDescriptor textureBindingLayout{};
  textureBindingLayout.entryCount = entries.size();
  textureBindingLayout.entries = entries.data();
  textureBindgroupLayout = device.CreateBindGroupLayout(&textureBindingLayout);
}

void Shader::LoadShader(std::string shaderCode, std::vector<wgpu::TextureFormat> outputFormats)
{
    FixTextureBindings(NumberOfTextures);
    uboLayout.Init();
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

    std::vector<wgpu::BindGroupLayout> bindGroupLayouts = {uboLayout.GetBindGroupLayout(), textureBindgroupLayout};

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
                                             .depthStencil = &depthStencilState,
                                             .multisample = {
                                                .count = 4,
                                                .mask = ~0u,
                                                .alphaToCoverageEnabled = false
                                             },
                                             .fragment = &fragmentState};

    m_Pipeline = device.CreateRenderPipeline(&descriptor);
}

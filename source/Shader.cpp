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

struct UBO {
  glm::mat4x4 projection;
  glm::mat4x4 view;
  glm::mat4x4 model;
};

UBO ubo{};
UniformLayout uboLayout(ubo, ubo.projection, ubo.view, ubo.model);

Shader::Shader()
{
}

void Shader::WriteToUBO()
{
    // --- time since first call (in seconds)
    using clock = std::chrono::steady_clock;
    static const auto t0 = clock::now();
    const float t = std::chrono::duration<float>(clock::now() - t0).count();

    // --- camera & projection (unchanged)
    const float fovDeg = 60.0f;
    const float aspect = 16.0f / 9.0f;
    const float zNear  = 0.1f;
    const float zFar   = 100.0f;

    ubo.projection = glm::perspectiveLH_ZO(glm::radians(fovDeg), aspect, zNear, zFar);

    const glm::vec3 eye    = {0.0f, 0.0f, 0.0f};
    const glm::vec3 target = {0.0f, 0.0f, 1.0f}; // +Z forward
    const glm::vec3 up     = {0.0f, 1.0f, 0.0f}; // +Y up
    ubo.view = glm::lookAtLH(eye, target, up);

    // --- model: translate then rotate -> spin in place at (0,1,10)
    const glm::vec3 pos = {0.0f, 0.0f, 3.0f};
    const float degPerSec = 45.0f;                 // tweak me
    const float angle = glm::radians(degPerSec) * t;

    ubo.model = glm::mat4(1.0f);
    ubo.model = glm::translate(ubo.model, pos);    // move to position
    ubo.model = glm::rotate(ubo.model, angle, {0.0f, 1.0f, 0.0f}); // spin around +Y

    uboLayout.pack(ubo);
}


Shader::~Shader()
{

}

wgpu::BindGroup& Shader::GetBindGroup()
{
  return uboLayout.GetBindGroup();
}

void Shader::LoadShader(std::string shaderCode, std::vector<wgpu::TextureFormat> outputFormats)
{
    uboLayout.Init();
    WriteToUBO();
    Vertex v{};
    VertexBufferLayout vertexLayout{v, v.position, v.normal};

    

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

    wgpu::PipelineLayoutDescriptor  layoutDesc = {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = &uboLayout.GetBindGroupLayout();
    m_Layout = device.CreatePipelineLayout(&layoutDesc);


    wgpu::RenderPipelineDescriptor descriptor{  .layout = m_Layout,
                                                .vertex = {
                                                  .module = shaderModule,
                                                  .bufferCount = 1,
                                                  .buffers = &vertexLayout.vertexBufferLayout
                                                },
                                             .depthStencil = &depthStencilState,
                                             .fragment = &fragmentState};

    m_Pipeline = device.CreateRenderPipeline(&descriptor);
}

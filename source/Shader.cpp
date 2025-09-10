#include <glm/glm.hpp>

#include <Shader.hpp>
#include <Logger.hpp>
#include <Init.hpp>
#include <VertexBufferLayout.hpp>
#include <Mesh.hpp>

Shader::Shader()
{
  
}

Shader::~Shader()
{
}

void Shader::LoadShader(std::string shaderCode, std::vector<wgpu::TextureFormat> outputFormats)
{
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


    wgpu::RenderPipelineDescriptor descriptor{.vertex = {
                                                  .module = shaderModule,
                                                  .bufferCount = 1,
                                                  .buffers = &vertexLayout.vertexBufferLayout
                                                },
                                             .depthStencil = &depthStencilState,
                                             .fragment = &fragmentState};

    m_Pipeline = device.CreateRenderPipeline(&descriptor);
}

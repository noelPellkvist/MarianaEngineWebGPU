#include <Shader.hpp>
#include <Logger.hpp>
#include <Init.hpp>

Shader::Shader()
{

}

Shader::~Shader()
{

}

void Shader::LoadShader(std::string shaderCode, std::vector<wgpu::TextureFormat> outputFormats)
{
    wgpu::ShaderSourceWGSL wgsl{{.code = shaderCode.c_str()}};
    wgpu::ShaderModuleDescriptor shaderModuleDescriptor{.nextInChain = &wgsl};

    wgpu::ShaderModule shaderModule =
    device.CreateShaderModule(&shaderModuleDescriptor);

    wgpu::ColorTargetState colorTargetState{.format = outputFormats[0]};

    wgpu::FragmentState fragmentState{
      .module = shaderModule, .targetCount = 1, .targets = &colorTargetState};

    wgpu::RenderPipelineDescriptor descriptor{.vertex = {.module = shaderModule},
                                             .fragment = &fragmentState};

    m_Pipeline = device.CreateRenderPipeline(&descriptor);
}

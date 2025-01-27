#include "Shader.hpp"
#include "../GlobalVaribles.hpp"

Shader::Shader(std::string shaderName, wgpu::TextureFormat targetFormat)
{
    const char shaderCode[] = R"(
            @vertex fn vertexMain(@builtin(vertex_index) i : u32) ->
              @builtin(position) vec4f {
                const pos = array(vec2f(0, 1), vec2f(-1, -1), vec2f(1, -1));
                return vec4f(pos[i], 0, 1);
            }
            @fragment fn fragmentMain() -> @location(0) vec4f {
                return vec4f(1, 0, 0, 1);
            }
        )";


    wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
    wgslDesc.code = shaderCode;

    wgpu::ShaderModuleDescriptor shaderModuleDescriptor{
        .nextInChain = &wgslDesc};
    wgpu::ShaderModule shaderModule =
        device.CreateShaderModule(&shaderModuleDescriptor);

    wgpu::ColorTargetState colorTargetState{.format = targetFormat};

    wgpu::FragmentState fragmentState{.module = shaderModule,
                                      .targetCount = 1,
                                      .targets = &colorTargetState};

    wgpu::RenderPipelineDescriptor descriptor{
        .vertex = {.module = shaderModule},
        .fragment = &fragmentState};
    m_Pipeline = device.CreateRenderPipeline(&descriptor);
}

Shader::~Shader()
{
    
}

void Shader::CreateRenderPipeline()
{

}
#include "Shader.hpp"
#include "../Buffers/VertexBuffer.hpp"
#include "../GlobalVaribles.hpp"
#include "../Logging.hpp"

#include <glm.hpp>

Shader::Shader(std::string shaderName, wgpu::TextureFormat targetFormat)
{
    CreateRenderPipeline(targetFormat);
}

Shader::~Shader()
{
    
}

void Shader::CreateRenderPipeline(wgpu::TextureFormat targetFormat)
{
    const char shaderCode[] = R"(
            struct VertexInput {
                @location(0) position: vec3f,
                @location(1) normal: vec3f,
                @location(2) color: vec4f,  // Not used in the fragment shader as per your request
                @location(3) uv: vec2f,
            };

            @vertex fn vertexMain(input: VertexInput) ->
              @builtin(position) vec4f {
                return vec4f(input.position.xyz, 1);
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

    VertexBufferLayoutData d = BuildVertexLayout({{{"POSITION", LayoutEntryType::Float32x3},
                                                   {"NORMAL", LayoutEntryType::Float32x3},
                                                   {"COLOR", LayoutEntryType::Float32x4},
                                                   {"TEXTCOORD_0", LayoutEntryType::Float32x2}}});

    std::vector<glm::vec3> pos = { {0, 1, 0}, {-1, -1, 0}, {1, -1, 0} };
    std::vector<glm::vec3> nor = { {0, 1, 0}, {-1, -1, 0}, {1, -1, 0} };
    std::vector<glm::vec4> col = { {1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}};
    std::vector<glm::vec2> uv = { {1, 0}, {0, 1}, {0, 0}};

    std::vector<VertexAttribute> data = {
        {"POSITION", pos.data(), 3},
        {"NORMAL", nor.data(), 3},
        {"COLOR", col.data(), 3},
        {"TEXTCOORD_0", uv.data(), 3},
    };
    vertexBuffer = CreateVertexBuffer(d, data);

    wgpu::RenderPipelineDescriptor descriptor{
        .vertex = {.module = shaderModule,
                   .bufferCount = 1,
                   .buffers = &d.vertexBufferLayout},
        .fragment = &fragmentState};
    m_Pipeline = device.CreateRenderPipeline(&descriptor);
}
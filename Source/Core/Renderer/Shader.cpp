#include "Shader.hpp"
#include "../GlobalVaribles.hpp"
#include "../Logging.hpp"
#include "../Resources.hpp"

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
    std::string shaderCode = Resources::LoadString("Shaders/standard.wgsl");


    wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
    wgslDesc.code = shaderCode.c_str();

    wgpu::ShaderModuleDescriptor shaderModuleDescriptor{
        .nextInChain = &wgslDesc};
    wgpu::ShaderModule shaderModule =
        device.CreateShaderModule(&shaderModuleDescriptor);

    wgpu::ColorTargetState colorTargetState{.format = targetFormat};

    wgpu::FragmentState fragmentState{.module = shaderModule,
                                      .targetCount = 1,
                                      .targets = &colorTargetState};

    vertexLayoutData = BuildVertexLayout({{{"POSITION", LayoutEntryType::Float32x3}}});

    // std::vector<glm::vec3> pos = { {0, 1, -1}, {-1, -1, -1}, {1, -1, -1} };

    // std::vector<VertexAttribute> data = {
    //     {"POSITION", pos.data(), pos.size()}
    // };
    // vertexBuffer = CreateVertexBuffer(data);

    UBOData = InitUBO();

    wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.label = wgpu::StringView("Built_in_PBR_Pipeline");
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &UBOData.bindGroupLayout;

    wgpu::PipelineLayout pipelineLayout = device.CreatePipelineLayout(&pipelineLayoutDesc);

    wgpu::RenderPipelineDescriptor descriptor{
        .layout = pipelineLayout,
        .vertex = {.module = shaderModule,
                   .bufferCount = 1,
                   .buffers = &vertexLayoutData.vertexBufferLayout},
        .fragment = &fragmentState};
    m_Pipeline = device.CreateRenderPipeline(&descriptor);
}

wgpu::Buffer Shader::CreateVertexBuffer(const std::vector<VertexAttribute>& data) const
{
    return CreateRawVertexBuffer(vertexLayoutData, data);
}
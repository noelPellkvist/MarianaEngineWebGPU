#include "Shader.hpp"
#include "../GlobalVaribles.hpp"
#include "../Logging.hpp"
#include "../Resources.hpp"

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include "../Components/Transform.hpp"
#include "../Components/Material.hpp"

Shader::Shader(const std::string& shaderName, wgpu::TextureFormat targetFormat)
{
    CreateRenderPipeline(targetFormat, shaderName);
}

Shader::~Shader()
{
    
}

void Shader::CreateRenderPipeline(wgpu::TextureFormat targetFormat, const std::string& shaderName)
{
    std::string shaderCode = Resources::LoadString("/Shaders/" + shaderName);


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

    vertexLayoutData = BuildVertexLayout({{{"POSITION", LayoutEntryType::Float32x3},
                                         {"NORMAL", LayoutEntryType::Float32x3}}});
    UBO ubo;

    float aspect = static_cast<float>(1336) / static_cast<float>(768);
    ubo.projectionMatrix = glm::perspective(glm::radians(60.0f), aspect, 0.01f, 100.0f);
    float currentTime = 0;
    ubo.viewMatrix = glm::lookAt(glm::vec3(15 * glm::sin(currentTime), 0, 15 * glm::cos(currentTime)), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    UBOData = CreateUniformBuffer(&ubo, sizeof(UBO), false);

    TransformBufferData transformBufferData;
    TransformData = CreateUniformBuffer(&transformBufferData, sizeof(TransformBufferData), true);

    MaterialBufferData materialComponent;
    MaterialBuffer = CreateUniformBuffer(&materialComponent, sizeof(MaterialBufferData), true);

    std::vector<wgpu::BindGroupLayout> bindgroupLayouts = {UBOData.bindGroupLayout, TransformData.bindGroupLayout, MaterialBuffer.bindGroupLayout};

    wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {};

    #if defined(__EMSCRIPTEN__)
        pipelineLayoutDesc.label = "Built_in_PBR_Pipeline";
    #else
    pipelineLayoutDesc.label = wgpu::StringView("Built_in_PBR_Pipeline");
    #endif

    
    pipelineLayoutDesc.bindGroupLayoutCount = bindgroupLayouts.size();
    pipelineLayoutDesc.bindGroupLayouts = bindgroupLayouts.data();

    wgpu::PipelineLayout pipelineLayout = device.CreatePipelineLayout(&pipelineLayoutDesc);

    wgpu::DepthStencilState depthStencilState = {};
    depthStencilState.depthCompare = wgpu::CompareFunction::LessEqual;
    depthStencilState.depthWriteEnabled = true;
    depthStencilState.format = wgpu::TextureFormat::Depth24Plus;
    depthStencilState.stencilReadMask = 0;
    depthStencilState.stencilWriteMask = 0;

    wgpu::RenderPipelineDescriptor descriptor{
        .layout = pipelineLayout,
        .vertex = {.module = shaderModule,
                   .bufferCount = 1,
                   .buffers = &vertexLayoutData.vertexBufferLayout},
        .depthStencil = &depthStencilState,
        .fragment = &fragmentState};
    m_Pipeline = device.CreateRenderPipeline(&descriptor);
}

wgpu::Buffer Shader::CreateVertexBuffer(const std::vector<VertexAttribute>& data) const
{
    return CreateRawVertexBuffer(vertexLayoutData, data);
}
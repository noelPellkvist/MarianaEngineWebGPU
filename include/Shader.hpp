#pragma once
#include <webgpu/webgpu_cpp.h>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

class Shader
{
    public:
        Shader(uint8_t textureCount);
        ~Shader();

        void LoadShader(std::string shaderCode, std::vector<wgpu::TextureFormat> outputFormats);

        wgpu::RenderPipeline& GetPipeline() { return m_Pipeline; }
        const wgpu::BindGroup& GetBindGroup() const;
        const wgpu::BindGroup& GetModelBindGroup() const;

        wgpu::BindGroupLayout& GetTextureBindGroupLayout() { return textureBindgroupLayout; }

        void WriteToUBO(glm::mat4 view, glm::vec3 cameraPos, float aspect);
        void WriteToModel(glm::mat4 model);

    private:
        wgpu::RenderPipeline m_Pipeline;
        wgpu::PipelineLayout m_Layout;

        wgpu::BindGroupLayoutEntry textureBinding{};
        wgpu::BindGroupLayoutEntry textureBinding2{};
        wgpu::BindGroupLayoutEntry textureBinding3{};
        wgpu::BindGroupLayoutEntry textureBinding4{};
        wgpu::BindGroupLayoutEntry textureBinding5{};
        wgpu::BindGroupLayoutEntry samplerBinding{};
        wgpu::BindGroupLayout textureBindgroupLayout{};

        uint8_t NumberOfTextures;

        void FixTextureBindings(uint8_t NumberOfTextures);
};
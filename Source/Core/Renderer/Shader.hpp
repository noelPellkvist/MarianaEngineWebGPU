#pragma once
#include <vector>
#include <string>
#include <webgpu/webgpu_cpp.h>
#include "../Buffers/UniformBuffer.hpp"
#include "../Buffers/VertexBuffer.hpp"

class Shader
{
    public:
        Shader(const std::string& shaderName, wgpu::TextureFormat targetFormat);
        ~Shader();

        const wgpu::RenderPipeline& GetRenderPipeline() { return m_Pipeline; };
        const VertexBufferLayoutData& GetVertexLayout() { return vertexLayoutData; };

        std::map<std::string, VertexBufferEntry> GetRequiredVertexAttributes() {return vertexLayoutData.offsetMap;};

        wgpu::Buffer CreateVertexBuffer(const std::vector<VertexAttribute>& data) const;

        wgpu::Buffer vertexBuffer;
        UniformBuffer UBOData;
        UniformBuffer TransformData;

    private:
        wgpu::RenderPipeline m_Pipeline;
        VertexBufferLayoutData vertexLayoutData;

        void CreateRenderPipeline(wgpu::TextureFormat targetFormat, const std::string& shaderName);



        
};
#pragma once
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include <vector>
#include <UniformLayout.hpp>
#include <VertexBufferLayout.hpp>
#include <Renderpass.hpp>
#include <any>
#include <stdexcept>
#include <Texture.hpp>
#include <memory>

class IShader
{
    public:
        IShader(VertexBufferLayout vbl, uint8_t textureCount, const Renderpass& renderpass);
        virtual ~IShader();

        void* GetPipeline();
        void* GetBindGroup(uint32_t index);

        void* GetBindGroupLayout(uint32_t index);

        void LoadShader(std::string shaderCode);

        virtual void* GetMaterialBufferEntry() = 0;

        virtual void InitBuffers() = 0;

        uint8_t GetTextureCount() { return m_TextureCount; }

        virtual void UpdateMaterialBuffer(const std::any& data, uint32_t bufferIndex) = 0;

        virtual uint32_t GetMaterialDynamicOffset(uint32_t bufferIndex) = 0;
        virtual uint32_t GetTransformDynamicOffset(uint32_t bufferIndex) = 0;

        virtual void* GetUBOBindGroupLayoutEntry() = 0;
        virtual void* GetTransformBindGroupLayoutEntry() = 0;
        virtual void* GetMaterialBindGroupLayoutEntry() = 0;
        virtual void* GetCameraBindGroupLayoutEntry() = 0;

        virtual void* GetUBOBindGroupEntry() = 0;
        virtual void* GetTransformBindGroupEntry() = 0;
        virtual void* GetCameraBindGroupEntry() = 0;

    protected:
        struct Impl;
        std::unique_ptr<Impl> _impl;
        uint8_t m_TextureCount;
        VertexBufferLayout m_VertexLayout;
        const Renderpass& m_Renderpass;

        void FixMaterialBindingLayout();
        void FixBindingLayouts();
        void CreateBindgroups();
};

template<typename UBOLayout, typename TransformLayout, typename MaterialLayout, typename CameraLayout>
class Shader : public IShader
{
    public:
        Shader(UniformLayout<UBOLayout>& uboLayout,
               UniformLayout<TransformLayout>& transformLayout,
               UniformLayout<MaterialLayout>& materialLayout,
               UniformLayout<CameraLayout>& cameraLayout,
               VertexBufferLayout vertexLayout,
               uint8_t textureCount,
               const Renderpass& renderpass)
          : IShader(std::move(vertexLayout), textureCount, renderpass),
            m_UBOLayout(uboLayout),
            m_TransformLayout(transformLayout),
            m_MaterialLayout(materialLayout),
            m_CameraLayout(cameraLayout)
        {

        }
        ~Shader() = default;

        void InitBuffers() override
        {
            m_UBOLayout.Init(0);
            m_TransformLayout.Init(0);
            m_MaterialLayout.Init(0);
            m_CameraLayout.Init(0);
            FixBindingLayouts();
            CreateBindgroups();
        }

        void* GetMaterialBufferEntry() override
        {
            return m_MaterialLayout.GetBindGroupEntry();
        }

        void UpdateMaterialBuffer(const std::any& data, uint32_t bufferIndex) override
        {
            try
            {
                const MaterialLayout& d = std::any_cast<const MaterialLayout&>(data);
                m_MaterialLayout.pack(d, bufferIndex);
            }
            catch(const std::exception& e)
            {
                throw std::runtime_error("Material::UpdateMaterialProperties: bad any_cast - wrong type passed");
            }
            
        }

        uint32_t GetMaterialDynamicOffset(uint32_t bufferIndex) override
        {
            return m_MaterialLayout.GetUniformStride() * bufferIndex;
        }

        uint32_t GetTransformDynamicOffset(uint32_t bufferIndex) override
        {
            return m_TransformLayout.GetUniformStride() * bufferIndex;
        }

        void* GetUBOBindGroupLayoutEntry() override
        {
            return m_UBOLayout.GetBindGroupLayoutEntry();
        }

        void* GetTransformBindGroupLayoutEntry() override
        {
            return m_TransformLayout.GetBindGroupLayoutEntry();
        }

        void* GetMaterialBindGroupLayoutEntry() override
        {
            return m_MaterialLayout.GetBindGroupLayoutEntry();
        }

        void* GetCameraBindGroupLayoutEntry() override
        {
            return m_CameraLayout.GetBindGroupLayoutEntry();
        }

        void* GetUBOBindGroupEntry() override
        {
            return m_UBOLayout.GetBindGroupEntry();
        }
        
        void* GetTransformBindGroupEntry() override
        {
            return m_TransformLayout.GetBindGroupEntry();
        }

        void* GetCameraBindGroupEntry() override
        {
            return m_CameraLayout.GetBindGroupEntry();
        }

    private:
        UniformLayout<UBOLayout>& m_UBOLayout;
        UniformLayout<TransformLayout>& m_TransformLayout;
        UniformLayout<MaterialLayout>& m_MaterialLayout;
        UniformLayout<CameraLayout>& m_CameraLayout;
};
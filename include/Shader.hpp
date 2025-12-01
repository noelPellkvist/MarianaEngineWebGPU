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

enum CullMode
{
    NONE,
    FRONT,
    BACK
};

enum FillMode
{
    SOLID,
    WIREFRAME
};

enum CompareOp
{
    LESS,
    LESSEQUAL,
    ALWAYS
};

struct ShaderProperties
{
    bool doubleSided;
    CullMode cullMode;
    FillMode fillMode;

    bool depthTest;
    bool depthWrite;
    CompareOp compareOp;
};

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
        virtual uint32_t GetBufferDynamicOffset(uint32_t binding, uint32_t bufferIndex) = 0;

        virtual uint32_t GetBufferDynamicOffsets(uint32_t binding) = 0;
        virtual uint32_t GetBindingsCount() = 0;

        virtual void* GetBindGroupLayoutEntry(uint32_t binding) = 0;

        virtual void* GetBindGroupEntry(uint32_t binding) = 0;

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

template<typename... Layouts>
class Shader : public IShader
{
    public:
        Shader(UniformLayout<Layouts>&... layouts,
               VertexBufferLayout vertexLayout,
               uint8_t textureCount,
               const Renderpass& renderpass)
          : IShader(std::move(vertexLayout), textureCount, renderpass)
        {
            static_assert(sizeof...(Layouts) >= 1 && sizeof...(Layouts) <= 4,
              "Shader must have between 1 and 4 layout types.");
            (m_layouts.emplace_back(&layouts), ...);
        }
        ~Shader() = default;

        void InitBuffers() override
        {

            for (auto layout : m_layouts)
            {
                layout->Init(0);
            }

            FixBindingLayouts();
            CreateBindgroups();
        }

        void* GetMaterialBufferEntry() override
        {
            return m_layouts.back()->GetBindGroupEntry();
        }

        void UpdateMaterialBuffer(const std::any& data, uint32_t bufferIndex) override
        {
            try
            {
                m_layouts.back()->pack(data, bufferIndex);
            }
            catch(const std::exception& e)
            {
                throw std::runtime_error("Material::UpdateMaterialProperties: bad any_cast - wrong type passed");
            }
            
        }

        uint32_t GetMaterialDynamicOffset(uint32_t bufferIndex) override
        {
            return m_layouts.back()->GetUniformStride() * bufferIndex;
        }

        uint32_t GetBufferDynamicOffset(uint32_t binding, uint32_t bufferIndex) override
        {
            
            return m_layouts[binding]->IsDynamic() ? m_layouts[binding]->GetUniformStride() * bufferIndex : 0;
        }

        uint32_t GetBufferDynamicOffsets(uint32_t binding) override
        {
            return m_layouts[binding]->IsDynamic() ? 1 : 0;
        }

        void* GetBindGroupLayoutEntry(uint32_t binding) override
        {
            return m_layouts[binding]->GetBindGroupLayoutEntry();
        }

        void* GetBindGroupEntry(uint32_t binding) override
        {
            return m_layouts[binding]->GetBindGroupEntry();
        }

        uint32_t GetBindingsCount() override
        {
            return m_layouts.size();
        }

    private:
        std::vector<IUniformLayout*> m_layouts;
};
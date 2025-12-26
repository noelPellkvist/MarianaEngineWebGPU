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
        IShader(VertexBufferLayout vbl, std::vector<TextureType> textureTypes, const Renderpass& renderpass, bool material = true);
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

        bool HasMaterial() const { return hasMaterial; }

    protected:
        struct Impl;
        std::unique_ptr<Impl> _impl;
        VertexBufferLayout m_VertexLayout;
        std::vector<TextureType> m_TextureTypes;
        uint16_t m_TextureCount;
        const Renderpass& m_Renderpass;
        bool hasMaterial = true;

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
               std::vector<TextureType> textureTypes,
               const Renderpass& renderpass, bool hasMaterial = true)
          : IShader(std::move(vertexLayout), textureTypes, renderpass, hasMaterial)
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
                if (!layout->IsInitialized())
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

enum class ShaderResourceType {
    UniformBuffer,
    Texture,
    Sampler
};

struct UniformBufferResource {
    const char* name;
    uint32_t binding;
    UniformBufferLayout layout;
    struct Impl;
    std::shared_ptr<Impl> _impl;

    UniformBufferResource() = default;
    UniformBufferResource(const char* name, uint32_t binding, UniformBufferLayout layout);
    ~UniformBufferResource();
};

struct TextureResource {
    const char* name;
    uint32_t binding;
    TextureType textureType;
    Texture* texture = nullptr;
    ~TextureResource();
};

struct SamplerResource {
    const char* name;
    uint32_t binding;
    struct Impl;
    std::shared_ptr<Impl> _impl;

    SamplerResource() = default;
    SamplerResource(const char* name, uint32_t binding);
    ~SamplerResource();
};
#include <variant>
using ShaderResource = std::variant<
    UniformBufferResource,
    TextureResource,
    SamplerResource>;

class Material2;
class Shader2;
class Renderpass;

class BindGroup {
    friend class Shader2;
    friend class Material2;
public:
    BindGroup();
    ~BindGroup() = default;

    BindGroup& AddUniformBuffer(const char* name, uint32_t binding, UniformBufferLayout layout);

    BindGroup& AddTexture(const char* name, uint32_t binding, TextureType type);

    BindGroup& AddSampler(const char* name, uint32_t binding);

    const std::vector<ShaderResource>& GetResources() const { return resources; }

private:
    std::vector<ShaderResource> resources;
    struct Impl;
    std::shared_ptr<Impl> _impl;
    void* GetBindGroup();
};



class Shader2 {
public:
    friend class Material2;
    friend class Renderpass;
    Shader2(const char* name); 
    ~Shader2();

    BindGroup& Group(uint32_t index)
    {
        if (index >= m_BindGroups.size())
            m_BindGroups.resize(index + 1);
        return m_BindGroups[index];
    }

    template <typename T>
    Shader2& WriteToBuffer(const std::string& name, const T& data, uint32_t index)
    {
        auto it = m_BindGroupLayoutMap.find(name);
        if (it == m_BindGroupLayoutMap.end())
            return *this;

        ShaderResource* res = it->second;
        if (!std::holds_alternative<UniformBufferResource>(*res))
            return *this;

        auto& ubr = std::get<UniformBufferResource>(*res);

        _writeToBuffer(&ubr, ubr.layout.Pack(data), index);

        return *this;
    }

    Shader2& SetMaterialGroup(uint32_t index) { m_MaterialIndex = index; return *this; }
    Shader2& SetVertexStructLayout(VertexBufferLayout vbl) { m_VertexLayout = vbl; return *this; }
    Shader2& SetWGSL(std::string src) { m_ShaderSource = src; return *this; }
    Shader2& SetRenderpass(Renderpass* pass) { m_Renderpass = pass; return *this; }
    Shader2& Build();

private:
    std::string m_Name;
    Renderpass* m_Renderpass = nullptr;
    std::string m_ShaderSource;
    std::vector<BindGroup> m_BindGroups;
    VertexBufferLayout m_VertexLayout;
    uint32_t m_MaterialIndex = 5;
    struct Impl;
    std::unique_ptr<Impl> _impl;
    std::unordered_map<std::string, ShaderResource*> m_BindGroupLayoutMap;

    void BuildBindgroupLayouts();
    void BuildBindgroups();
    void BuildBindgroupFromLayout(BindGroup& bg, int i);
    
    void* GetPipeline();
    void* GetBindGroup(uint32_t index);
    uint32_t GetGroupCount() const { return static_cast<uint32_t>(m_BindGroups.size()); }

    void _writeToBuffer(UniformBufferResource* res, std::vector<std::byte>& data, uint32_t index);
};

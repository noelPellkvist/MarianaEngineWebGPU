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

enum class ShaderResourceType {
    UniformBuffer,
    Texture,
    Sampler
};

struct UniformBufferResource {
    const char* name;
    uint32_t binding;
    UniformBufferLayout layout;
    uint32_t group;
    uint32_t dynamicBufferOffsetIndex = 0;
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
private:
    std::vector<ShaderResource> resources;
    struct Impl;
    std::shared_ptr<Impl> _impl;
    void* GetBindGroup();
    uint32_t m_DynamicBufferCount = 0;
    
public:
    BindGroup();
    ~BindGroup() = default;

    BindGroup& AddUniformBuffer(const char* name, uint32_t binding, UniformBufferLayout layout);

    BindGroup& AddTexture(const char* name, uint32_t binding, TextureType type);

    BindGroup& AddSampler(const char* name, uint32_t binding);

    std::vector<ShaderResource>& GetResources() { return resources; }

    const uint32_t GetDynamicBufferCount() const { return m_DynamicBufferCount; }


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

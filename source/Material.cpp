#include <Material.hpp>
#include "Init.hpp"
#include <FileReader.hpp>
#include <webgpu/webgpu_cpp.h>


Material2::Material2()
{

}

Material2::~Material2()
{

}

Material2& Material2::InitFromShader(Shader2& shader)
{
    m_Bindgroup.resources = shader.m_BindGroups[shader.m_MaterialIndex].resources;
    m_Shader = &shader;
    for (ShaderResource& res : m_Bindgroup.resources)
    {
        if (std::holds_alternative<BufferResource>(res))
        {
            const BufferResource& br = std::get<BufferResource>(res);
            m_ResourceMap[br.name] = &res;
        }
        else if (std::holds_alternative<TextureResource>(res))
        {
            const TextureResource& tr = std::get<TextureResource>(res);
            m_ResourceMap[tr.name] = &res;
        }
        else if (std::holds_alternative<SamplerResource>(res))
        {
            const SamplerResource& sr = std::get<SamplerResource>(res);
            m_ResourceMap[sr.name] = &res;
        }
    }
    return *this;
}

 Material2& Material2::SetTexture(std::string name, Texture& texture)
 {
    ShaderResource& m = *m_ResourceMap[name];

    if (std::holds_alternative<TextureResource>(m))
    {
        std::get<TextureResource>(m).texture = &texture;
    }
    return *this;
 }

void* Material2::GetBindGroup()
{
    return m_Bindgroup.GetBindGroup();
}

Material2& Material2::Build()
{
    m_Shader->BuildBindgroupFromLayout(m_Bindgroup, m_Shader->m_MaterialIndex);
    return *this;
}


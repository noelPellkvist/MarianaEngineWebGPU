#include "Scene.hpp"
#include "Renderer/Mesh.hpp"
#include "Renderer/Transform.hpp"

Scene::Scene(const std::string& name, wgpu::TextureFormat targetFormat) : m_Name(name), m_RenderTargetFormat(targetFormat),
m_RenderLayer(targetFormat, "standard.wgsl")
{

}

Scene::~Scene()
{

}

void Scene::DrawAllObjects(wgpu::SurfaceTexture& surfaceTexture)
{
    auto view = m_Entities.view<const Transform, const Mesh>();
    for(auto [entity, transform, mesh]: view.each()) {
        m_RenderLayer.Render(surfaceTexture, mesh);
    }   
}

entt::entity Scene::CreateGameobject(const std::string& name)
{
    entt::entity n = m_Entities.create();
    m_Entities.emplace<Transform>(n);
    return n;
}
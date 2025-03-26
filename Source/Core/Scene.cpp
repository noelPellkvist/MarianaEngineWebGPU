#include "Scene.hpp"
#include "Components/Mesh.hpp"
#include "Components/Transform.hpp"
#include "Components/Relationship.hpp"
#include "Logging.hpp"


Scene::Scene(const std::string& name) : m_Name(name)
{
    //m_Entities.on_construct<Transform>().connect<&RenderSystem::RegisterComponent>(rs);
}

Scene::~Scene()
{

}

entt::entity Scene::CreateGameobject(const std::string& name, entt::entity parent)
{
    entt::entity n = m_Entities.create();
    m_Entities.emplace<Transform>(n, name.c_str());
    auto& relation = m_Entities.emplace<Relationship>(n);
    relation.SetParent(parent, n, m_Entities);
    return n;
}
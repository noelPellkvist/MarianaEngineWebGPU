#include "Scene.hpp"

Scene::Scene(const std::string& name) : m_Name(name)
{

}

Scene::~Scene()
{

}

entt::entity Scene::CreateEntity(const std::string& name)
{
    return m_Entities.create();
}
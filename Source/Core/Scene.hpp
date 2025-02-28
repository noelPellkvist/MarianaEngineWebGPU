#pragma once
#include <entt/entt.hpp>
#include <string>

class Scene
{
    public:
        Scene(const std::string& name);
        ~Scene();

        entt::entity CreateEntity(const std::string& name);
    private:
        std::string m_Name;
        entt::registry m_Entities;
};
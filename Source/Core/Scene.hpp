#pragma once
#include <entt/entt.hpp>
#include <string>
#include "Renderer/Layer.hpp"
#include "Renderer/Shader.hpp"

class Scene
{
    public:
        Scene(const std::string& name);
        ~Scene();

        entt::registry& GetEntities() { return m_Entities; }

        entt::entity CreateGameobject(const std::string& name);

        void SetCreationCallback(void (*b) ());



    private:
        std::string m_Name;
        entt::registry m_Entities;

};
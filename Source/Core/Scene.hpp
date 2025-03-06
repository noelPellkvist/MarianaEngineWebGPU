#pragma once
#include <entt/entt.hpp>
#include <string>
#include "Renderer/Layer.hpp"
#include "Renderer/Shader.hpp"

class Scene
{
    public:
        Scene(const std::string& name, wgpu::TextureFormat targetFormat);
        ~Scene();

        void DrawAllObjects(wgpu::SurfaceTexture& surfaceTexture);
        entt::registry& GetEntities() { return m_Entities; }

        std::vector<Shader>& GetShaders() { return m_RenderLayer.GetShaders(); }

        entt::entity CreateGameobject(const std::string& name);
    private:
        std::string m_Name;
        entt::registry m_Entities;
        RenderLayer m_RenderLayer;
        wgpu::TextureFormat m_RenderTargetFormat;
};
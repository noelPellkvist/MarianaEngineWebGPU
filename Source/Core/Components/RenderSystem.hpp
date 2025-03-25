#pragma once
#include "../Scene.hpp"
#include <vector>
#include "../Renderer/GUI.hpp"

class RenderSystem
{
    public:
        RenderSystem(Scene& scene, wgpu::TextureFormat targetFormat);
        ~RenderSystem();

        std::vector<Shader>& GetShaders() { return m_RenderLayer.GetShaders(); };

        void Draw(wgpu::SurfaceTexture& surface);
        void RegisterComponent(entt::registry& reg, entt::entity e);
    private:
        Scene& scene;
        RenderLayer m_RenderLayer;
        wgpu::Buffer m_TransformsBuffer;
        int entityCount = 0;

        uint32_t ceilToNextMultiple(uint32_t value, uint32_t step);

        void OnNewTransform(entt::registry& registry, entt::entity entity);



    
};
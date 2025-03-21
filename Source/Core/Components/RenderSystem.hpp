#pragma once
#include "../Scene.hpp"
#include <vector>

class RenderSystem
{
    public:
        RenderSystem(Scene& scene, wgpu::TextureFormat targetFormat);
        ~RenderSystem();

        std::vector<Shader>& GetShaders() { return m_RenderLayer.GetShaders(); };

        void Draw(wgpu::SurfaceTexture& surface);
        void RegisterComponent(entt::entity e);
    private:
        Scene& scene;
        RenderLayer m_RenderLayer;
        wgpu::Buffer m_TransformsBuffer;
        uint32_t entityCount = 0;

        void CreateTransformsBuffer();
        uint32_t ceilToNextMultiple(uint32_t value, uint32_t step);

        void OnNewTransform(entt::registry& registry, entt::entity entity);



    private:
        struct TransformData
        {
            glm::mat4 modelMatrix;
            glm::mat4 normalMatrix;
        };
};
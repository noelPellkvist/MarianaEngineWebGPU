#pragma once
#include "../Scene.hpp"
#include <vector>
#include "../Renderer/GUI.hpp"
#include "Material.hpp"
#include "Mesh.hpp"
#include "Transform.hpp"

class RenderSystem
{
    public:
        RenderSystem(Scene& scene, wgpu::TextureFormat targetFormat);
        ~RenderSystem();

        std::vector<Shader>& GetShaders() { return m_RenderLayer.GetShaders(); };

        void Draw(wgpu::SurfaceTexture& surface);
        void RegisterComponent(entt::registry& reg, entt::entity e);
        void AddMaterial(MaterialComponent& m) { m_Materials.push_back(m); }
        void AddMaterials(std::vector<MaterialComponent>& m);
        size_t GetMaterialCount() { return m_Materials.size(); }

    private:
        Scene& scene;
        RenderLayer m_RenderLayer;
        int entityCount = 0;
        std::vector<MaterialComponent> m_Materials;

        uint32_t ceilToNextMultiple(uint32_t value, uint32_t step);

        void RecalculateTransforms();
        void TraverseTransforms(entt::entity entity, const TransformBufferData& parentData);
};
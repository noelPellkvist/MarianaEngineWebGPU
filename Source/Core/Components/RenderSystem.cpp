#include "RenderSystem.hpp"
#include "Mesh.hpp"
#include "Transform.hpp"
#include "../GlobalVaribles.hpp"
#include "../Logging.hpp"

RenderSystem::RenderSystem(Scene& scene, wgpu::TextureFormat targetFormat) : scene(scene), m_RenderLayer(targetFormat, "standard.wgsl")
{
}

RenderSystem::~RenderSystem()
{

}

void RenderSystem::Draw(wgpu::SurfaceTexture& surfaceTexture)
{
    m_RenderLayer.Render(surfaceTexture, scene.GetEntities());
}

uint32_t RenderSystem::ceilToNextMultiple(uint32_t value, uint32_t step) {
    uint32_t divide_and_ceil = value / step + (value % step == 0 ? 0 : 1);
    return step * divide_and_ceil;
}

void RenderSystem::RegisterComponent(entt::registry& reg, entt::entity e)
{
    reg.get<Transform>(e).dataIndex = entityCount;
    Logging::PrintSuccess("Registred components with index" + std::to_string(entityCount));
    struct ModelData
    {
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        glm::mat4 normalMatrix = glm::mat4(1.0f);
    } model;
    if (entityCount == 0)
    {
        model.modelMatrix = glm::translate(model.modelMatrix, glm::vec3(-4,0,0));
    }
    else if (entityCount == 1)
        model.modelMatrix = glm::translate(model.modelMatrix, glm::vec3(4,0,0));
    m_RenderLayer.GetShaders()[0].TransformData.UpdateValue(&model, sizeof(ModelData), entityCount);
    entityCount++;
}

void OnNewTransform(entt::registry& registry, entt::entity entity)
{
    Logging::PrintSuccess("Registred new transform");
}
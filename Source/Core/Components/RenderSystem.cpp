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
    std::vector<wgpu::TextureView> views = {surfaceTexture.texture.CreateView()};
    m_RenderLayer.Render(views, scene.GetEntities());
}

uint32_t RenderSystem::ceilToNextMultiple(uint32_t value, uint32_t step) {
    uint32_t divide_and_ceil = value / step + (value % step == 0 ? 0 : 1);
    return step * divide_and_ceil;
}

void RenderSystem::RegisterComponent(entt::registry& reg, entt::entity e)
{
    reg.get<Transform>(e).dataIndex = entityCount;
    Logging::PrintSuccess("Registred components with index" + std::to_string(entityCount));
    TransformBufferData model;

    glm::mat3 normalMat3 = glm::transpose(glm::inverse(glm::mat3(model.modelMatrix)));
    glm::mat4 normalMatrix = glm::mat4(1.0f); // Start with an identity matrix
    normalMatrix[0] = glm::vec4(normalMat3[0], 0.0f); // First row of normal matrix
    normalMatrix[1] = glm::vec4(normalMat3[1], 0.0f); // Second row of normal matrix
    normalMatrix[2] = glm::vec4(normalMat3[2], 0.0f); // Third row of normal matrix
    model.normalMatrix = normalMatrix;
    m_RenderLayer.GetShaders()[0].TransformData.UpdateValue(&model, sizeof(TransformBufferData), entityCount);
    entityCount++;
}

void OnNewTransform(entt::registry& registry, entt::entity entity)
{
    Logging::PrintSuccess("Registred new transform");
}
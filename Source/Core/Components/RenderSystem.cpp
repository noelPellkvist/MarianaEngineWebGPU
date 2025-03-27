#include "RenderSystem.hpp"
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

    RecalculateTransforms();

    m_RenderLayer.Render(views, scene.GetEntities());
}

void RenderSystem::RecalculateTransforms()
{
    auto view = scene.GetEntities().view<Transform, const Relationship, Dirty>();
    
    for(auto&& [entity, transform, relations]: view.each()) {
        scene.GetEntities().remove<Dirty>(entity);
        m_RenderLayer.GetShaders()[0].TransformData.UpdateValue(&transform.data, sizeof(TransformBufferData), transform.dataIndex);
        const auto children = relations.GetAllChildren(scene.GetEntities());
        for (const auto& child : children)
            TraverseTransforms(child, transform.data);
    }
}

void RenderSystem::TraverseTransforms(entt::entity entity, const TransformBufferData& parentData)
{
    Transform& thisTransform = scene.GetEntities().get<Transform>(entity);
    scene.GetEntities().remove<Dirty>(entity);

    thisTransform.data.modelMatrix = parentData.modelMatrix * glm::translate(glm::mat4(1.0f), thisTransform.position) *
                                     glm::mat4_cast(thisTransform.rotation) *
                                     glm::scale(glm::mat4(1.0f), thisTransform.scale);

    glm::mat3 normalMat3 = glm::transpose(glm::inverse(glm::mat3(thisTransform.data.modelMatrix)));
    glm::mat4 normalMatrix = glm::mat4(1.0f); 
    normalMatrix[0] = glm::vec4(normalMat3[0], 0.0f); 
    normalMatrix[1] = glm::vec4(normalMat3[1], 0.0f); 
    normalMatrix[2] = glm::vec4(normalMat3[2], 0.0f); 
    thisTransform.data.normalMatrix = normalMatrix;
    m_RenderLayer.GetShaders()[0].TransformData.UpdateValue(&thisTransform.data, sizeof(TransformBufferData), thisTransform.dataIndex);

    const Relationship& rel = scene.GetEntities().get<Relationship>(entity);
    const auto& children = rel.GetAllChildren(scene.GetEntities());
        for (const auto& child : children)
            TraverseTransforms(child, thisTransform.data);
}

void RenderSystem::AddMaterials(std::vector<MaterialComponent>& m) 
{ 
    m_Materials.insert(m_Materials.end(), m.begin(), m.end());
    for(int i = 0; i < m.size(); i++)
    {
        m_RenderLayer.GetShaders()[0].MaterialBuffer.UpdateValue(&m[i].uniform, sizeof(MaterialBufferData), m[i].dataIndex);
    }
 }

uint32_t RenderSystem::ceilToNextMultiple(uint32_t value, uint32_t step) {
    uint32_t divide_and_ceil = value / step + (value % step == 0 ? 0 : 1);
    return step * divide_and_ceil;
}

void RenderSystem::RegisterComponent(entt::registry& reg, entt::entity e)
{
    auto& t = reg.get<Transform>(e);
    t.dataIndex = entityCount;

    TransformBufferData& model = t.data;
    size_t apa = sizeof(TransformBufferData);

    model.modelMatrix = glm::translate(glm::mat4(1.0f), t.position)
         * glm::mat4_cast(t.rotation)
         * glm::scale(glm::mat4(1.0f), t.scale);

    glm::mat3 normalMat3 = glm::transpose(glm::inverse(glm::mat3(model.modelMatrix)));
    glm::mat4 normalMatrix = glm::mat4(1.0f); 
    normalMatrix[0] = glm::vec4(normalMat3[0], 0.0f); 
    normalMatrix[1] = glm::vec4(normalMat3[1], 0.0f); 
    normalMatrix[2] = glm::vec4(normalMat3[2], 0.0f); 
    model.normalMatrix = normalMatrix;
    m_RenderLayer.GetShaders()[0].TransformData.UpdateValue(&t.data, sizeof(TransformBufferData), t.dataIndex);
    entityCount++;
}
#pragma once

#include "Editor/Windows/EditorWindow.hpp"
#include <ECS.hpp>

#include <functional>
#include <string>
#include <unordered_map>

class InspectorWindow final : public EditorWindow
{
public:
    explicit InspectorWindow(GUI& gui);

    void Draw() override;
    void SetInspectedEntity(const Scene& scene, Entity entity);
    void ClearInspectedEntity();

private:
    using ComponentDrawer = std::function<void(Entity&)>;

    const Scene* m_scene = nullptr;
    Entity m_entity;
    uint64_t m_entityID = static_cast<uint64_t>(-1);
    std::unordered_map<std::string, ComponentDrawer> m_componentDrawers;

    void RegisterComponentDrawers();
    void DrawEntityInspector();
    void DrawLocalTRS(Entity& entity);
    void DrawHiddenComponentsAndTags(Entity entity);
};
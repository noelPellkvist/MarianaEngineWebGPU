#pragma once

#include <Editor/Windows/EditorWindow.hpp>
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <functional>
#include <string>
#include <unordered_map>

class AssetManager2;

// Shows the selected entity's components and materials, or an asset (material, texture, mesh,
// ...) opened from there or from the Loaded Assets window, with a way back to the entity.
class InspectorWindow final : public EditorWindow
{
public:
    InspectorWindow(GUI& gui, AssetManager2& assets, const Scene& scene);

    void Draw() override;
    void SetInspectedEntity(const Scene& scene, Entity entity);
    void ClearInspectedEntity();
    void InspectAsset(AssetID id) { m_asset = id; }
    // How the inspector selects an entity (from "used by" lists).
    void SetSelectEntity(std::function<void(Entity)> selectEntity) { m_selectEntity = std::move(selectEntity); }

private:
    using ComponentDrawer = std::function<void(Entity&)>;

    AssetManager2& m_assets;
    const Scene* m_scene = nullptr;
    AssetID m_asset; // shown instead of the entity while valid
    std::function<void(Entity)> m_selectEntity;
    Entity m_entity;
    uint64_t m_entityID = static_cast<uint64_t>(-1);
    std::unordered_map<std::string, ComponentDrawer> m_componentDrawers;

    void RegisterComponentDrawers();
    void DrawEntityInspector();
    void DrawLocalTRS(Entity& entity);
    void DrawHiddenComponentsAndTags(Entity entity);
};
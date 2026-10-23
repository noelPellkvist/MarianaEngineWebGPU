#pragma once

#include <Editor/Windows/EditorWindow.hpp>
#include <Editor/Windows/InspectorComponents/AnimationComponent.hpp>
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class AssetManager2;
class UndoStack;

// Shows the selected entity's components and materials, or an asset (material, texture, mesh,
// ...) opened from there or from the Loaded Assets window, with a way back to the entity.
class InspectorWindow final : public EditorWindow
{
public:
    InspectorWindow(GUI& gui, AssetManager2& assets, const Scene& scene, UndoStack& undo);

    void Draw() override;
    void SetInspectedEntity(const Scene& scene, Entity entity);
    void ClearInspectedEntity();
    void InspectAsset(AssetID id) { m_asset = id; }
    // How the inspector selects an entity (from "used by" lists).
    void SetSelectEntity(std::function<void(Entity)> selectEntity) { m_selectEntity = std::move(selectEntity); }
    // What the Animator section's graph buttons do.
    void SetAnimatorActions(InspectorComponents::AnimatorActions actions) { m_animatorActions = std::move(actions); }

    // Other modules' sections: the drawer of a component (by its registered name), and entries of
    // the Add Component menu (grouped by `menu`, offered while `available` says so).
    using ComponentDrawer = std::function<void(Entity&)>;
    void SetComponentDrawer(const std::string& componentName, ComponentDrawer drawer) { m_componentDrawers[componentName] = std::move(drawer); }
    struct ComponentOption
    {
        std::string menu;
        std::string label;
        std::function<bool(Entity)> available;
        std::function<void(Entity)> add;
    };
    void AddComponentOption(ComponentOption option) { m_addOptions.push_back(std::move(option)); }
    // Runs `fn` after the entity's sections are drawn (adding or removing components while they are
    // listed isn't safe).
    void Defer(std::function<void()> fn) { m_deferred.push_back(std::move(fn)); }

private:
    AssetManager2& m_assets;
    UndoStack& m_undo;
    const Scene* m_scene = nullptr;
    AssetID m_asset; // shown instead of the entity while valid
    std::function<void(Entity)> m_selectEntity;
    InspectorComponents::AnimatorActions m_animatorActions;
    Entity m_entity;
    uint64_t m_entityID = static_cast<uint64_t>(-1);
    std::unordered_map<std::string, ComponentDrawer> m_componentDrawers;
    std::vector<ComponentOption> m_addOptions;
    std::vector<std::function<void()>> m_deferred;

    void RegisterComponentDrawers();
    void DrawEntityInspector();
    void DrawHiddenComponentsAndTags(Entity entity);
    void DrawAddComponent();
};
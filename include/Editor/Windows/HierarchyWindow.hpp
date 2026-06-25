#pragma once

#include "Editor/Windows/EditorWindow.hpp"
#include <ECS.hpp>

#include <filesystem>
#include <functional>
#include <string>

class HierarchyWindow final : public EditorWindow
{
public:
    using SelectEntityCallback = std::function<void(uint64_t)>;
    using SpawnGlbCallback = std::function<void(const std::filesystem::path&)>;

    HierarchyWindow(
        GUI& gui,
        const Scene& scene,
        const uint64_t& selectedEntityID,
        SelectEntityCallback selectEntity,
        SpawnGlbCallback spawnGlb);

    void Draw() override;

private:
    const Scene& m_scene;
    const uint64_t& m_selectedEntityID;
    SelectEntityCallback m_selectEntity;
    SpawnGlbCallback m_spawnGlb;

    void DrawEntityNode(Entity e);
    void DrawDropTarget();
};
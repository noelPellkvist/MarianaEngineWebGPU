#include "Editor/Windows/InspectorWindow.hpp"

#include "Editor/Windows/InspectorComponents/AnimationComponent.hpp"
#include "Editor/Windows/InspectorComponents/LocalTRS.hpp"
#include "Editor/Windows/InspectorComponents/MeshComponent.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

InspectorWindow::InspectorWindow(GUI& gui)
    : EditorWindow("Inspector", gui, true)
{
    RegisterComponentDrawers();
}

void InspectorWindow::SetInspectedEntity(const Scene& scene, Entity entity)
{
    m_scene = &scene;
    m_entity = entity;
    m_entityID = entity.IsValid() ? entity.RawId() : static_cast<uint64_t>(-1);
}

void InspectorWindow::ClearInspectedEntity()
{
    m_scene = nullptr;
    m_entity = Entity{};
    m_entityID = static_cast<uint64_t>(-1);
}

void InspectorWindow::RegisterComponentDrawers()
{
    m_componentDrawers["LocalTRS"] = &InspectorComponents::DrawLocalTRS;
    m_componentDrawers["AnimationPlayer"] = &InspectorComponents::DrawAnimationComponent;
    m_componentDrawers["MeshComponent"] = &InspectorComponents::DrawMeshComponent;
}

void InspectorWindow::Draw()
{
    if (!m_open)
        return;

    if (ImGui::Begin(m_name.c_str(), &m_open))
    {
        if (!m_scene || !m_entity.IsValid() || m_entityID == static_cast<uint64_t>(-1))
            ImGui::Text("Select an entity to show it here");
        else
            DrawEntityInspector();
    }
    ImGui::End();
}

void InspectorWindow::DrawEntityInspector()
{
    ImGui::TextDisabled("Name");
    ImGui::SameLine(0, 16);
    ImGui::SetNextItemWidth(-1);

    char entityName[128] = {};
    const char* currentName = m_entity.GetName();
    if (currentName)
        std::strncpy(entityName, currentName, sizeof(entityName) - 1);

    ImGui::InputText("##entity_name", entityName, sizeof(entityName));
    m_entity.SetName(entityName);

    ImGui::Separator();

    m_scene->ForEachComponent(m_entity, [&](const ComponentView& component) {
        if (component.isTag)
            return;

        const char* name = (component.name && *component.name) ? component.name : "<unnamed>";
        auto drawer = m_componentDrawers.find(name);
        if (drawer != m_componentDrawers.end())
            drawer->second(m_entity);
    });

    DrawHiddenComponentsAndTags(m_entity);
}

void InspectorWindow::DrawHiddenComponentsAndTags(Entity entity)
{
    std::vector<std::string> hiddenComponents;
    std::vector<std::string> tags;

    m_scene->ForEachComponent(entity, [&](const ComponentView& component) {
        const char* name = (component.name && *component.name) ? component.name : "<unnamed>";

        if (component.isTag)
        {
            tags.emplace_back(name);
            return;
        }

        if (m_componentDrawers.find(name) == m_componentDrawers.end())
            hiddenComponents.emplace_back(name);
    });

    std::sort(hiddenComponents.begin(), hiddenComponents.end());
    std::sort(tags.begin(), tags.end());

    ImGui::Separator();
    ImGui::TextDisabled("Tags");
    if (tags.empty())
    {
        ImGui::TextDisabled("None");
    }
    else
    {
        for (const std::string& tag : tags)
            ImGui::BulletText("%s", tag.c_str());
    }

    ImGui::Separator();
    ImGui::TextDisabled("Hidden Components");
    if (hiddenComponents.empty())
    {
        ImGui::TextDisabled("None");
    }
    else
    {
        for (const std::string& component : hiddenComponents)
            ImGui::BulletText("%s", component.c_str());
    }
}

#pragma once

#include <Editor/Windows/EditorWindow.hpp>
#include <Mariana/Scene/ECS.hpp>
#include <glm/glm.hpp>

#include <filesystem>
#include <string>

class InspectorWindow;
class UndoStack;
class PhysicsWorld;

// The editor's side of MarianaPhysics (the library itself has no UI).
namespace PhysicsEditor
{
    // The inspector's Rigid Body and collider sections (with the body's live state while the
    // simulation runs) and their Add Component entries. Edits are undoable.
    void RegisterInspector(InspectorWindow& inspector, UndoStack& undo, const PhysicsWorld& world);

    // Sizes and centres the colliders on `entity` to its mesh's bounds; false when it has no mesh.
    bool FitCollidersToMesh(Entity entity);

    // Collider outlines over a viewport image (`imageMin`, `imageSize` in screen pixels): those of
    // `selected` and everything below it, or of every entity when `all`.
    void DrawColliders(const Scene& scene, Entity selected, bool all, const glm::mat4& viewProj, glm::vec2 imageMin, glm::vec2 imageSize);
}

// The simulation's settings (gravity, steps, collision layers and which collide), saved to the
// project's /Physics.json, the outline toggle and live numbers.
class PhysicsWindow final : public EditorWindow
{
public:
    PhysicsWindow(GUI& gui, PhysicsWorld& world, std::filesystem::path settingsFile);
    void Draw() override;

    bool showAllColliders = false;

private:
    PhysicsWorld& m_World;
    std::filesystem::path m_File;
    std::string m_Status;
};

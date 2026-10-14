#pragma once
#include <Mariana/Application.hpp>
#include <Mariana/Rendering/Renderer.hpp>
#include <Editor/GUI.hpp>
#include <Editor/UndoStack.hpp>
#include <Mariana/Scene/ECS.hpp>
#include <Editor/Windows/EditorWindow.hpp>
#include <Mariana/Rendering/SceneRenderer.hpp>
#include <Mariana/Rendering/Effect.hpp>
#include <Mariana/Rendering/BuiltinAssets.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

class InspectorWindow;

// The editor: windows, picking and the gizmo around a scene drawn by the engine's renderer.
class EditorApp : public Application
{
    public:
        EditorApp();
        ~EditorApp() override;

    protected:
        void OnStart() override;
        void OnUpdate(float deltaTime) override;
        void OnRender() override;
        void OnGUI() override;
        void OnShutdown() override;

    private:
        GUI gui;
        UndoStack undo;
        std::unordered_map<std::string, std::unique_ptr<EditorWindow>> EditorWindows;
        InspectorWindow* inspectorWindow = nullptr;
        // Declared first: the scene renderer and the effects remove their passes from its graph.
        Renderer renderer;
        Scene scene;
        SceneRenderer sceneRenderer;
        EffectStack effects;
        // Systems query `scene`, so they are declared after it to be destroyed first.
        System animationSystem;
        uint64_t selectedEntityID = -1;
        Entity selectedEntity;

        void OnWindowResized(int w, int h);
        void SpawnPrefab(const std::filesystem::path& path);
        Entity SpawnPrimitive(BuiltinMesh shape, const char* name);

        void SelectEntity(uint64_t id);
        void DeselectEntity();

        void DrawTopMenu();
        void DrawMat4(const char* id, float m[16], bool editable = true, float speed = 0.05f, const char* fmt = "%.3f");
        bool DrawGizmo(glm::mat4& transform, const glm::mat4& view, const glm::mat4& proj);
};

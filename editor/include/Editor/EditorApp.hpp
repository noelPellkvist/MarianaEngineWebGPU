#pragma once
#include <Mariana/Application.hpp>
#include <Editor/GUI.hpp>
#include <Editor/UndoStack.hpp>
#include <Editor/ViewRenderer.hpp>
#include <Mariana/Scene/ECS.hpp>
#include <Editor/Windows/EditorWindow.hpp>
#include <Mariana/Rendering/SceneRenderer.hpp>
#include <Mariana/Rendering/SceneCamera.hpp>
#include <Mariana/Rendering/BuiltinAssets.hpp>
#include <Mariana/Physics/PhysicsWorld.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class InspectorWindow;
class AnimationGraphWindow;
class PhysicsWindow;
class ThumbnailCache;
class ViewportWindow;

// The editor: windows, picking and the gizmo around a scene drawn by the engine's renderer. The scene
// is shown in two windows, Scene (the editor camera, with picking and the gizmo) and Game (the first
// camera in the scene), each a view of the same scene renderer drawn into a texture.
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
        // Outlives the windows that show its thumbnails.
        std::unique_ptr<ThumbnailCache> thumbnails;
        std::unordered_map<std::string, std::unique_ptr<EditorWindow>> EditorWindows;
        InspectorWindow* inspectorWindow = nullptr;
        AnimationGraphWindow* graphWindow = nullptr;
        ViewportWindow* sceneWindow = nullptr;
        ViewportWindow* gameWindow = nullptr;
        Scene scene;
        SceneRenderer sceneRenderer;
        // The scene's simulation: bodies only exist while playing (Play and Stop reset it).
        std::unique_ptr<PhysicsWorld> physics;
        PhysicsWindow* physicsWindow = nullptr;
        std::vector<DebugLine> debugLines; // PhysicsWorld::DebugDraw's, kept to reuse the memory
        // The two views of sceneRenderer, drawn only while their windows show. The game view is made
        // the first time its window shows.
        std::unique_ptr<ViewRenderer> sceneView;
        std::unique_ptr<ViewRenderer> gameView;
        SceneCamera gameCamera;
        // Every entity with a CameraComponent, found each frame; the Game window shows the first.
        std::vector<Entity> sceneCameras;
        // Systems query `scene`, so they are declared after it to be destroyed first.
        System animationSystem;
        System cameraSystem;
        uint64_t selectedEntityID = -1;
        Entity selectedEntity;
        // The scene and undo history as they were when Play was pressed; Stop puts them back.
        SceneSnapshot playSnapshot;
        UndoStack playUndo;
        bool restoreOnStop = false;

        void OnWindowResized(int w, int h);
        void SpawnPrefab(const std::filesystem::path& path);
        Entity SpawnPrimitive(BuiltinMesh shape, const char* name);
        // A primitive with a collider to match, dynamic or static.
        Entity SpawnPhysicsShape(BuiltinMesh shape, const char* name, bool dynamic);
        // A static ground (30 m across) under where shapes spawn, and a tower of dynamic boxes on it.
        Entity SpawnGround();
        void SpawnBoxStack();
        // A drivable car, motorcycle or tank (with a chase camera if the scene has none), and a
        // ground with ramps to drive on.
        Entity SpawnVehicle(VehicleType type);
        Entity SpawnPlayground();
        // A camera where the editor camera is, looking the same way.
        Entity SpawnCamera();

        void SelectEntity(uint64_t id);
        void DeselectEntity();

        void DrawTopMenu();
        // Play/Stop, Pause and Step in the middle of the menu bar, like Unity's toolbar: the game's
        // clock (gameClock) only runs in play mode, so animations hold still while editing.
        void DrawPlayControls();
        void TogglePlay();
        void TogglePause();
        // Stop: the scene goes back to what it was on Play (what changed while playing is undone;
        // edits to assets, like materials, stay).
        void RestoreScene();
        void DrawMat4(const char* id, float m[16], bool editable = true, float speed = 0.05f, const char* fmt = "%.3f");
        // Over the Scene window's image: the cameras' frustums, the selection's gizmo, picking.
        void DrawSceneOverlay(ViewportWindow& window);
        void DrawCameraFrustums(const ViewportWindow& window);
        bool DrawGizmo(glm::mat4& transform, const glm::mat4& view, const glm::mat4& proj, const ViewportWindow& window);
};

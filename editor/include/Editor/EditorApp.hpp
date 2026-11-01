#pragma once
#include <Mariana/Application.hpp>
#include <Editor/GUI.hpp>
#include <Editor/UndoStack.hpp>
#include <Editor/TransformUndo.hpp>
#include <Editor/ViewRenderer.hpp>
#include <Mariana/Scene/ECS.hpp>
#include <Editor/Windows/EditorWindow.hpp>
#include <Mariana/Rendering/SceneRenderer.hpp>
#include <Mariana/Rendering/SceneCamera.hpp>
#include <Mariana/Rendering/BuiltinAssets.hpp>
#include <Mariana/Importers/SceneFile.hpp>
#include <Mariana/Physics/PhysicsWorld.hpp>
#include <Mariana/Scripting/ScriptWorld.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class InspectorWindow;
class AnimationGraphWindow;
class PhysicsWindow;
class RenderPipelineWindow;
class ScriptChecker;
class ScriptEditorWindow;
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
        // Asks to save unsaved changes first.
        bool OnCloseRequested() override;

    private:
        GUI gui;
        UndoStack undo;
        TransformUndo gizmoUndo;
        // Outlives the windows that show its thumbnails.
        std::unique_ptr<ThumbnailCache> thumbnails;
        std::unordered_map<std::string, std::unique_ptr<EditorWindow>> EditorWindows;
        InspectorWindow* inspectorWindow = nullptr;
        AnimationGraphWindow* graphWindow = nullptr;
        RenderPipelineWindow* pipelineWindow = nullptr;
        ViewportWindow* sceneWindow = nullptr;
        ViewportWindow* gameWindow = nullptr;
        Scene scene;
        SceneRenderer sceneRenderer;
        // The scene's simulation: bodies only exist while playing (Play and Stop reset it).
        std::unique_ptr<PhysicsWorld> physics;
        std::unique_ptr<ScriptWorld> scripts;
        std::unique_ptr<ScriptChecker> scriptChecker;
        ScriptEditorWindow* scriptEditorWindow = nullptr;
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
        // Entities whose changes made while playing stay after Stop (true: their children's too).
        std::unordered_map<uint64_t, bool> keepOnStop;

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
        // A walking character (with a chase camera if the scene has none) and a course to try it on.
        Entity SpawnCharacter();
        Entity SpawnCharacterCourse();
        // A chain, a door, a spinner or a wrecking ball (0..3), in front of the camera.
        // 0 chain, 1 door, 2 spinner, 3 wrecking ball, 4 water, 5 water pool, 6 a dancing ragdoll
        // (facing the camera), on the ground ahead.
        Entity SpawnJointPreset(int which);
        // A camera following the driven entity (ChaseCamera.luau with these properties), unless the
        // scene has one.
        void AddChaseCamera(float distance, float height, float lookHeight);
        // What the editor opens with when the project has no /Scenes/Playground.scene (and what that
        // was saved from): the vehicle playground, a drivable car, dancers, a pool, jellies and a flag.
        void BuildStartupScene();

        // Scene files (SceneFile): the one being edited (empty until saved), and whether it changed
        // since: what saving would write (SceneFile::Signature, checked twice a second while the
        // game is stopped), or the asset files with edits Save Scene writes too.
        std::filesystem::path scenePath;
        uint64_t savedSignature = 0;
        bool signaturePending = true; // the next check takes the signature as the saved one
        bool sceneChanged = false;
        float signatureTimer = 0.0f;
        std::vector<std::filesystem::path> unsavedFiles; // .material, .animgraph and .pipeline files
        std::vector<std::filesystem::path> recentScenes;
        bool openScenePopup = false;
        bool saveScenePopup = false;
        bool savePromptPopup = false;
        // What waits for the scene to be saved or not (opening another, quitting).
        std::function<void()> afterSavePrompt;
        std::string sceneStatus;
        // Stops the game, empties the scene and forgets its history.
        void ClearScene();
        void NewScene();
        bool OpenScene(const std::filesystem::path& file);
        // Writes the asset files with edits (see unsavedFiles), then the scene with its settings.
        bool SaveScene(const std::filesystem::path& file);
        bool SaveModifiedAssets(std::vector<std::filesystem::path>& written);
        // The lighting and pipeline the scene is saved with.
        SceneSettings CurrentSettings() const;
        void UpdateModified();
        // Runs `then` at once when nothing is unsaved, else after asking (Save / Don't Save / Cancel).
        void AskToSave(std::function<void()> then);
        // The editor camera of each scene, in the project's .mariana/editor folder.
        std::filesystem::path EditorCameraFile() const;
        void SaveEditorCamera() const;
        bool LoadEditorCamera();
        // Save, or Save As when the scene has no file yet; on the web the file also goes to the
        // browser as a download (the page's own files are gone when it reloads).
        void SaveSceneFromMenu(bool saveAs);
        void OfferDownload(const std::filesystem::path& file);
        void RememberScene(const std::filesystem::path& file);
        bool SceneModified() const;
        void DrawScenePopups();
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

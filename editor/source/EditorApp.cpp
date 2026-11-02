#include <Editor/EditorApp.hpp>
#include <Editor/ThumbnailCache.hpp>
#include <Mariana/Foundation/FileReader.hpp>
#include <Editor/EditorCameraController.hpp>
#include <Mariana/Importers/GLTFLoader.hpp>
#include <Mariana/Importers/MaterialFile.hpp>
#include <Mariana/Importers/AnimationGraphFile.hpp>
#include <Mariana/Importers/SceneFile.hpp>
#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Foundation/Profiler.hpp>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif
#include <Mariana/Rendering/BuiltinAssets.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>
#include <Mariana/Rendering/RenderPipeline.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Rendering/SceneView.hpp>
#include <Mariana/Scene/ECS.hpp>
#include <Mariana/Scene/Prefab.hpp>
#include <Mariana/Animation/Animator.hpp>
#include <Mariana/Physics/Components.hpp>
#include <Mariana/Physics/PhysicsWorld.hpp>
#include <Mariana/Physics/VehiclePresets.hpp>
#include <Mariana/Physics/CharacterPresets.hpp>
#include <Mariana/Physics/JointPresets.hpp>
#include <Mariana/Physics/WaterPresets.hpp>
#include <Mariana/Physics/SoftBodyPresets.hpp>

#include "ImGuizmo.h"

#include <Editor/Windows/Stats.hpp>
#include <Editor/Windows/ProfilerWindow.hpp>
#include <Editor/Physics/PhysicsEditor.hpp>
#include <Editor/Scripting/ScriptEditor.hpp>
#include <Editor/Windows/ScriptEditorWindow.hpp>
#include <Mariana/ScriptTools/ScriptChecker.hpp>
#include <Mariana/ScriptTools/ScriptTypes.hpp>
#include <Editor/Windows/AnimationGraphWindow.hpp>
#include <Editor/Windows/AssetsExplorer.hpp>
#include <Editor/Windows/BuildWindow.hpp>
#include <Mariana/Export/Process.hpp>
#include <Mariana/Export/Project.hpp>
#include <Editor/Windows/HierarchyWindow.hpp>
#include <Editor/Windows/InspectorWindow.hpp>
#include <Editor/Windows/LoadedAssetsWindow.hpp>
#include <Editor/Windows/RenderPipelineWindow.hpp>
#include <Editor/Windows/LightingWindow.hpp>
#include <Editor/Windows/ViewportWindow.hpp>

#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>
#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>
#include <algorithm>
#include <array>
#include <cstdint>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
namespace fs = std::filesystem;

namespace
{
    const char* const kDefaultPipeline = "/Pipelines/Default.pipeline";

    // The .material file a material is kept in (its own, or the one it was extracted or saved to).
    fs::path MaterialFileOf(const AssetMetadata& meta)
    {
        if (meta.path.extension() == ".material")
            return meta.path;
        if (meta.source.extension() == ".material")
            return meta.source;
        return {};
    }
}

#pragma region Helpers

inline std::string ToString(const glm::vec3& v)
{
    std::ostringstream ss;
    ss << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return ss.str();
}

inline std::string ToString(const float& v)
{
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

inline std::string ToString(const int& v)
{
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

inline std::string ToString(const uint32_t& v)
{
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

inline std::string ToString(const uint64_t& v)
{
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

#pragma endregion

static ImGuizmo::OPERATION CurrentGizmoOperation = ImGuizmo::TRANSLATE;

EditorApp::EditorApp(fs::path projectFolder)
    : Application({ .title = "Mariana Engine Editor - " + projectFolder.filename().string(), .width = 1366, .height = 768, .iconPath = "/logo.png" }),
project(std::move(projectFolder)),
sceneRenderer(scene, assets),
gameCamera(input)
{
    Animator::RegisterComponents();
    SceneRenderer::RegisterComponents();
    PhysicsWorld::RegisterComponents();
    ScriptWorld::RegisterComponents();

    scene.UpdateComponentRegistry();

    cam = std::make_unique<EditorCameraController>(input);

    // The scene view's graph has the scene passes, ambient occlusion and the post-process chain
    // that ends in the image the Scene window shows (sized to it).
    sceneRenderer.Init();
    sceneView = std::make_unique<ViewRenderer>(sceneRenderer, true);
    GLTF::GLTFLoader::RegisterLoader(assets);
    MaterialFile::RegisterLoader(assets);
    AnimationGraphFile::RegisterLoader(assets);
    ScriptAsset::RegisterLoader(assets);
    SceneFile::RegisterPrefabLoader(assets);

    // The effects, their settings, the exposure and the shadow settings come from a pipeline file.
    // The game view copies the scene view's.
    RenderPipelineDesc pipeline;
    std::string pipelineError;
    if (RenderPipelineDesc::Load(kDefaultPipeline, pipeline, pipelineError))
        pipeline.Apply(sceneView->Effects(), sceneRenderer, cam.get());
    else
        Logger::Error("Could not load the render pipeline: " + pipelineError);

    animationSystem = Animator::CreateSystem(scene);
    cameraSystem = scene.CreateSystem<CameraComponent, WorldXform>([this](Entity e, CameraComponent&, WorldXform& world, float) {
        if (world.active) // an inactive camera doesn't show the game
            sceneCameras.push_back(e);
    });

    scene.Update(0.f);
    m_Window.RegisterResizeCallback([this](int w, int h) {
        this->OnWindowResized(w, h);
    });
}

EditorApp::~EditorApp()
{

}

void EditorApp::OnWindowResized(int w, int h)
{
    // Minimized: nothing to render into.
    if (w <= 0 || h <= 0)
        return;

    // Only the UI is drawn on the screen; the views follow their windows' sizes.
    m_Window.GetSurface();
}

void EditorApp::SpawnPrefab(const fs::path& path)
{
    AssetHandle<Prefab> model = assets.Load<Prefab>(path);
    std::shared_ptr<Prefab> prefab = model.Get();
    if (!prefab)
    {
        Logger::Error("Failed to spawn prefab from: " + path.string());
        return;
    }

    Entity spawned = scene.Instantiate(*prefab, path.stem().string().c_str());
    scene.Update(0.0f);
    DeselectEntity();
    SelectEntity(spawned.RawId());
    Logger::Info("Spawned prefab from: " + path.string());
}

Entity EditorApp::SpawnPrimitive(BuiltinMesh shape, const char* name)
{
    Entity spawned = BuiltinAssets::InstantiatePrimitive(scene, assets, shape, name);
    if (!spawned.IsValid())
        return {};

    // A few metres in front of the camera.
    const CameraInfo info = cam->GetCameraInfo();
    const glm::vec3 position = info.pos + glm::normalize(glm::vec3(info.invView[2])) * 5.0f;
    spawned.SetPosition(position.x, position.y, position.z);
    scene.Update(0.0f);
    DeselectEntity();
    SelectEntity(spawned.RawId());
    return spawned;
}

Entity EditorApp::SpawnEmpty()
{
    Entity spawned = scene.Instantiate("Entity");
    spawned.SetPosition(0.0f, 0.0f, 0.0f);
    scene.Update(0.0f);
    DeselectEntity();
    SelectEntity(spawned.RawId());
    return spawned;
}

Entity EditorApp::SpawnPhysicsShape(BuiltinMesh shape, const char* name, bool dynamic)
{
    Entity spawned = SpawnPrimitive(shape, name);
    if (!spawned.IsValid())
        return spawned;
    switch (shape)
    {
    case BuiltinMesh::Sphere: spawned.Add<SphereCollider>(SphereCollider{}); break;
    case BuiltinMesh::Capsule: spawned.Add<CapsuleCollider>(CapsuleCollider{}); break;
    case BuiltinMesh::Cylinder: spawned.Add<CylinderCollider>(CylinderCollider{}); break;
    default: spawned.Add<BoxCollider>(BoxCollider{}); break;
    }
    PhysicsEditor::FitCollidersToMesh(spawned);
    if (dynamic)
        spawned.Add<RigidBody>(RigidBody{});
    return spawned;
}

Entity EditorApp::SpawnGround()
{
    Entity ground = SpawnPhysicsShape(BuiltinMesh::Plane, "Ground", false);
    if (!ground.IsValid())
        return ground;
    const LocalTRS* trs = ground.TryGet<LocalTRS>();
    ground.SetPosition(trs->pos[0], 0.0f, trs->pos[2]);
    ground.SetScaleUniform(3.0f);
    scene.Update(0.0f);
    return ground;
}

void EditorApp::SpawnBoxStack()
{
    Entity ground = SpawnGround();
    if (!ground.IsValid())
        return;
    const LocalTRS* base = ground.TryGet<LocalTRS>();
    const float x = base->pos[0], z = base->pos[2];
    for (int i = 0; i < 8; ++i)
    {
        Entity box = SpawnPhysicsShape(BuiltinMesh::Cube, "Box", true);
        // Each a little turned, so the tower is a bit wobbly.
        const glm::quat turn = glm::angleAxis(glm::radians(7.0f * static_cast<float>(i)), glm::vec3(0.0f, 1.0f, 0.0f));
        box.SetPosition(x, 0.5f + 1.02f * static_cast<float>(i), z);
        box.SetRotationQuat(turn.x, turn.y, turn.z, turn.w);
    }
    scene.Update(0.0f);
}

Entity EditorApp::SpawnVehicle(VehicleType type)
{
    // In front of the camera on the ground (or whatever is below), facing away from it.
    const CameraInfo info = cam->GetCameraInfo();
    glm::vec3 forward = glm::vec3(info.invView[2]);
    forward.y = 0.0f;
    forward = glm::length(forward) > 1e-4f ? glm::normalize(forward) : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 position = info.pos + forward * 8.0f;
    position.y = 1.5f;
    const glm::quat facing = glm::angleAxis(std::atan2(forward.x, forward.z), glm::vec3(0.0f, 1.0f, 0.0f));
    Entity vehicle = VehiclePresets::Spawn(scene, assets, type, position, facing);
    // The keys drive it, and a chase camera for the Game view follows (unless the scene has one).
    ScriptWorld::AddScript(vehicle, assets, "/Scripts/VehicleDriver.luau");
    AddChaseCamera(7.0f, 2.5f, 1.0f);
    scene.Update(0.0f);
    DeselectEntity();
    SelectEntity(vehicle.RawId());
    return vehicle;
}

Entity EditorApp::SpawnCharacter()
{
    const CameraInfo info = cam->GetCameraInfo();
    glm::vec3 forward = glm::vec3(info.invView[2]);
    forward.y = 0.0f;
    forward = glm::length(forward) > 1e-4f ? glm::normalize(forward) : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 position = info.pos + forward * 6.0f;
    position.y = 0.0f;
    const glm::quat facing = glm::angleAxis(std::atan2(forward.x, forward.z), glm::vec3(0.0f, 1.0f, 0.0f));
    Entity character = CharacterPresets::Spawn(scene, assets, position, facing);
    ScriptWorld::AddScript(character, assets, "/Scripts/CharacterDriver.luau");
    AddChaseCamera(5.0f, 2.0f, 1.4f);
    scene.Update(0.0f);
    DeselectEntity();
    SelectEntity(character.RawId());
    return character;
}

Entity EditorApp::SpawnJointPreset(int which)
{
    // On the ground in front of the camera.
    const CameraInfo info = cam->GetCameraInfo();
    glm::vec3 forward = glm::vec3(info.invView[2]);
    forward.y = 0.0f;
    forward = glm::length(forward) > 1e-4f ? glm::normalize(forward) : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 at = info.pos + forward * (which == 4 || which == 5 ? 16.0f : 8.0f);
    at.y = 0.0f;
    const glm::quat facing = glm::angleAxis(std::atan2(-forward.x, -forward.z), glm::vec3(0.0f, 1.0f, 0.0f));
    Entity spawned = which == 0 ? JointPresets::SpawnChain(scene, assets, at + glm::vec3(0.0f, 6.0f, 0.0f))
                   : which == 1 ? JointPresets::SpawnDoor(scene, assets, at)
                   : which == 2 ? JointPresets::SpawnSpinner(scene, assets, at)
                   : which == 3 ? JointPresets::SpawnWreckingBall(scene, assets, at)
                   : which == 4 ? WaterPresets::SpawnWater(scene, assets, at + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(10.0f, 2.0f, 10.0f))
                   : which == 5 ? WaterPresets::SpawnPool(scene, assets, at)
                   : which == 6 ? CharacterPresets::SpawnRagdoll(scene, assets, at, facing)
                   : which == 7 ? SoftBodyPresets::SpawnFlag(scene, assets, at, facing)
                   : which == 8 ? SoftBodyPresets::SpawnDrape(scene, assets, at)
                   : which == 9 ? SoftBodyPresets::SpawnJelly(scene, assets, at)
                   : SoftBodyPresets::SpawnBalloon(scene, assets, at);
    if (!spawned.IsValid())
        return spawned;
    scene.Update(0.0f);
    DeselectEntity();
    SelectEntity(spawned.RawId());
    return spawned;
}

Entity EditorApp::SpawnCharacterCourse()
{
    const CameraInfo info = cam->GetCameraInfo();
    Entity course = CharacterPresets::SpawnCourse(scene, assets, glm::vec3(info.pos.x, 0.0f, info.pos.z));
    scene.Update(0.0f);
    return course;
}

void EditorApp::AddChaseCamera(float distance, float height, float lookHeight)
{
    bool following = false;
    scene.Each<Scripts>([&](Entity, const Scripts& s) {
        for (const ScriptSlot& slot : s.scripts)
            if (const std::shared_ptr<ScriptAsset>& script = slot.script.Get(); script && script->file.stem() == "ChaseCamera")
                following = true;
    });
    if (following)
        return;
    Entity camera = SpawnCamera();
    camera.SetName("Chase Camera");
    ScriptWorld::AddScript(camera, assets, "/Scripts/ChaseCamera.luau", {
        ScriptValue{ "distance", ScriptValueType::Number, distance },
        ScriptValue{ "height", ScriptValueType::Number, height },
        ScriptValue{ "lookHeight", ScriptValueType::Number, lookHeight } });
}

Entity EditorApp::SpawnPlayground()
{
    const CameraInfo info = cam->GetCameraInfo();
    Entity playground = VehiclePresets::SpawnPlayground(scene, assets, glm::vec3(info.pos.x, 0.0f, info.pos.z));
    scene.Update(0.0f);
    return playground;
}

Entity EditorApp::SpawnCamera()
{
    // The view matrix's inverse holds the camera's right, up and forward axes; an entity's camera
    // looks along its +z, so the same rotation makes it see what the editor camera sees.
    const CameraInfo info = cam->GetCameraInfo();
    const glm::quat rotation = glm::normalize(glm::quat_cast(glm::mat3(info.invView)));

    Entity spawned = scene.Instantiate("Camera");
    spawned.SetPosition(info.pos.x, info.pos.y, info.pos.z);
    spawned.SetRotationQuat(rotation.x, rotation.y, rotation.z, rotation.w);
    CameraComponent camera;
    camera.fov = cam->FieldOfView();
    spawned.Add<CameraComponent>(camera);
    scene.Update(0.0f);
    DeselectEntity();
    SelectEntity(spawned.RawId());
    return spawned;
}

void EditorApp::OnStart()
{
    Logger::Info("Starting");
    gui.InitGui(m_Window);

    // The project's physics settings, if it has them.
    const fs::path physicsFile = project / "Physics.json";
    PhysicsSettings physicsSettings;
    std::error_code exists;
    if (std::string error; fs::exists(physicsFile, exists) && !physicsSettings.Load(physicsFile, error))
        Logger::Warning(error);
    physics = std::make_unique<PhysicsWorld>(scene, physicsSettings);
    auto physicsEditor = std::make_unique<PhysicsWindow>(gui, *physics, physicsFile);
    physicsWindow = physicsEditor.get();
    EditorWindows["Physics"] = std::move(physicsEditor);
    scripts = std::make_unique<ScriptWorld>(scene, assets, gameClock);
    scripts->SetPhysics(physics.get());
    // Checking scripts against the API's types (every component is registered by now), and the same
    // types written out for VS Code.
    scriptChecker = std::make_unique<ScriptChecker>(ScriptTypes::Definitions());
    if (!scriptChecker->DefinitionErrors().empty())
        Logger::Error("The script API's types don't load: " + scriptChecker->DefinitionErrors());
#ifndef __EMSCRIPTEN__
    ScriptEditor::WriteProjectTypes(project);
#endif
    auto scriptEditor = std::make_unique<ScriptEditorWindow>(gui, assets, *scriptChecker);
    scriptEditorWindow = scriptEditor.get();
    scriptEditor->onSaved = [this](const fs::path& file) { OfferDownload(file); };
    EditorWindows["Script Editor"] = std::move(scriptEditor);

    // Traces go to the project's cache folder (and, on the web, to the browser as a download).
    EditorWindows["Profiler"] = std::make_unique<ProfilerWindow>(gui, [this](const std::string& trace) {
        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char name[64];
        std::strftime(name, sizeof(name), "profile-%Y%m%d-%H%M%S.json", std::localtime(&now));
        const fs::path file = project / ".mariana" / "profiles" / name;
        std::error_code ec;
        fs::create_directories(file.parent_path(), ec);
        std::ofstream(file, std::ios::binary) << trace;
        Logger::Info("Saved a trace of the profiler's frames to " + file.string());
        OfferDownload(file);
    });
    EditorWindows["Stats"] = std::make_unique<StatsWindow>(gui, [this]() { EditorWindows["Profiler"]->SetOpen(true); });
    thumbnails = std::make_unique<ThumbnailCache>(project, input);
    auto graphEditor = std::make_unique<AnimationGraphWindow>(gui, assets, undo, scene, project);
    graphWindow = graphEditor.get();
    EditorWindows["Animation Graph"] = std::move(graphEditor);

    auto assetsExplorer = std::make_unique<AssetsExplorer>(gui, project, *thumbnails);
    assetsExplorer->onOpen = [this](const fs::path& file) {
        std::string extension = file.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        if (extension == ".animgraph")
            if (AssetHandle<AnimationGraph> graph = assets.Load<AnimationGraph>(file); graph.Get())
                graphWindow->Open(graph);
        if (extension == ".scene")
            OpenScene(file);
        if (extension == ".luau" || extension == ".lua")
            scriptEditorWindow->Open(file);
    };
    assetsExplorer->createActions.push_back({ "Animation Graph", [this](const fs::path& folder) {
        graphWindow->CreateGraph(folder / "New Animation Graph.animgraph", nullptr);
    } });
    assetsExplorer->createActions.push_back({ "Script", [](const fs::path& folder) {
        ScriptEditor::CreateScript(folder, "NewScript");
    } });
    EditorWindows["Assets"] = std::move(assetsExplorer);
    auto inspector = std::make_unique<InspectorWindow>(gui, assets, scene, undo);
    inspectorWindow = inspector.get();
    inspector->SetAnimatorActions({
        [this](const AssetHandle<AnimationGraph>& graph) { graphWindow->Open(graph); },
        [this](Entity entity) {
            Animator* animator = entity.Get<Animator>();
            const std::string name = entity.GetName();
            const fs::path file = project / "Animations" / ((name.empty() ? std::string("Animator") : name) + ".animgraph");
            if (AssetHandle<AnimationGraph> graph = graphWindow->CreateGraph(file, animator); graph.Get() && animator)
                animator->graph = graph;
        } });
    inspector->SetSelectEntity([this](Entity entity) {
        DeselectEntity();
        SelectEntity(entity.RawId());
    });
    PhysicsEditor::RegisterInspector(*inspector, undo, *physics, assets, scene);
    ScriptEditor::RegisterInspector(*inspector, undo, *scripts, *scriptChecker, assets, project,
                                    [this](const fs::path& file) { scriptEditorWindow->Open(file); });
    inspector->AddComponentOption({ "Rendering", "Camera",
        [](Entity e) { return !e.TryGet<CameraComponent>(); },
        [](Entity e) { e.Add<CameraComponent>(CameraComponent{}); } });
    // While playing: whether Stop keeps what the game did to this entity.
    inspector->SetEntityHeader([this](Entity& e) {
        // Its own switches (the Hierarchy greys out what isn't drawn), as undo steps.
        auto toggle = [this](Entity entity, bool on, bool active) {
            const uint64_t id = entity.RawId();
            auto set = [this, id, active](bool value) {
                Entity target = scene.FromId(id);
                if (target.IsValid())
                    active ? target.SetActive(value) : target.SetVisible(value);
            };
            set(on);
            const char* verb = active ? (on ? "Activate " : "Deactivate ") : (on ? "Show " : "Hide ");
            undo.Push(verb + std::string(entity.GetName()), [set, on]() { set(!on); }, [set, on]() { set(on); });
        };
        bool active = e.IsActiveSelf();
        if (ImGui::Checkbox("Active", &active))
            toggle(e, active, true);
        ImGui::SetItemTooltip("An inactive entity (and everything below it) isn't drawn or simulated, and its scripts and animation don't run.");
        ImGui::SameLine();
        bool visible = e.IsVisibleSelf();
        if (ImGui::Checkbox("Visible", &visible))
            toggle(e, visible, false);
        ImGui::SetItemTooltip("A hidden entity (and everything below it) isn't drawn; it still runs.");
        if (!gameClock.IsPlaying())
            return;
        const auto found = keepOnStop.find(e.RawId());
        bool keep = found != keepOnStop.end();
        if (ImGui::Checkbox("Keep changes after Stop", &keep))
        {
            if (keep)
                keepOnStop[e.RawId()] = false;
            else
                keepOnStop.erase(e.RawId());
        }
        ImGui::SetItemTooltip("Stop puts the scene back as it was on Play; this entity keeps its components as the game leaves them.");
        if (keep)
        {
            ImGui::SameLine();
            bool children = keepOnStop[e.RawId()];
            if (ImGui::Checkbox("With children", &children))
                keepOnStop[e.RawId()] = children;
        }
    });
    EditorWindows["Inspector"] = std::move(inspector);
    EditorWindows["Loaded Assets"] = std::make_unique<LoadedAssetsWindow>(gui, assets, [this](AssetID id) {
        inspectorWindow->InspectAsset(id);
        EditorWindows["Inspector"]->SetOpen(true);
    });
    auto pipelineEditor = std::make_unique<RenderPipelineWindow>(gui, sceneView->Effects(), sceneRenderer, *cam, kDefaultPipeline);
    pipelineWindow = pipelineEditor.get();
    pipelineWindow->MarkSaved(); // the constructor's caller applied it
    EditorWindows["Render Pipeline"] = std::move(pipelineEditor);
    EditorWindows["Lighting"] = std::make_unique<LightingWindow>(gui, sceneRenderer.lighting);
    auto builds = std::make_unique<BuildWindow>(gui, project);
    buildWindow = builds.get();
    EditorWindows["Build"] = std::move(builds);
    auto hierarchy = std::make_unique<HierarchyWindow>(
        gui,
        scene,
        selectedEntityID,
        [this](uint64_t id) {
            DeselectEntity();
            SelectEntity(id);
        },
        [this](const fs::path& gltfPath) {
            SpawnPrefab(gltfPath);
        });
    // Entities that keep their play-mode changes are marked, and can be marked from their menu.
    hierarchy->decorate = [this](Entity e) {
        const auto found = keepOnStop.find(e.RawId());
        if (found == keepOnStop.end())
            return;
        const char* name = e.GetName() ? e.GetName() : "";
        const ImVec2 min = ImGui::GetItemRectMin();
        const float x = min.x + ImGui::GetTreeNodeToLabelSpacing() + ImGui::CalcTextSize(name).x + ImGui::GetStyle().ItemSpacing.x;
        ImGui::GetWindowDrawList()->AddText(ImVec2(x, min.y + ImGui::GetStyle().FramePadding.y), ImGui::GetColorU32(ImGuiCol_CheckMark),
                                            found->second ? "keep (with children)" : "keep");
    };
    hierarchy->contextMenu = [this](Entity e) {
        if (!gameClock.IsPlaying() && ImGui::MenuItem("Save as Prefab"))
        {
            // Into the project's Prefabs folder, named like the entity (replacing one of that name).
            std::string name = e.GetName() && *e.GetName() ? e.GetName() : "Prefab";
            for (char& c : name)
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_' && c != ' ')
                    c = '_';
            const fs::path file = project / "Prefabs" / (name + ".prefab");
            std::error_code ec;
            fs::create_directories(file.parent_path(), ec);
            std::string error;
            if (SceneFile::SavePrefab(e, assets, file, error))
            {
                Logger::Info("Saved " + file.string());
                OfferDownload(file);
            }
            else
                Logger::Error(error);
        }
        if (!gameClock.IsPlaying())
            return;
        const bool kept = keepOnStop.contains(e.RawId());
        if (ImGui::MenuItem("Keep changes after Stop", nullptr, kept))
        {
            if (kept)
                keepOnStop.erase(e.RawId());
            else
                keepOnStop[e.RawId()] = false;
        }
    };
    EditorWindows["Hierarchy"] = std::move(hierarchy);

    auto sceneViewport = std::make_unique<ViewportWindow>("Scene", gui);
    sceneWindow = sceneViewport.get();
    sceneWindow->image = [this]() { return sceneView->Image(); };
    sceneWindow->overlay = [this](ViewportWindow& window) { DrawSceneOverlay(window); };
    EditorWindows["Scene"] = std::move(sceneViewport);
    auto gameViewport = std::make_unique<ViewportWindow>("Game", gui);
    gameWindow = gameViewport.get();
    gameWindow->image = [this]() -> Texture* { return gameView && !sceneCameras.empty() ? gameView->Image() : nullptr; };
    gameWindow->emptyText = "No camera in the scene";
    EditorWindows["Game"] = std::move(gameViewport);

    if (auto* editorCam = dynamic_cast<EditorCameraController*>(cam.get())) {
        editorCam->SetPosition({-10.0f, 3.0f, -10.0f});
        editorCam->SetYawPitch(glm::half_pi<float>() / 2, -glm::half_pi<float>() / 5);
    } 

#if defined(__EMSCRIPTEN__)
    // Leaving the page with unsaved changes asks first (UpdateModified sets the flag).
    EM_ASM({
        window.addEventListener('beforeunload', (event) => {
            if (Module.marianaUnsaved) {
                event.preventDefault();
                event.returnValue = '';
            }
        });
    });
#endif

    OpenStartScene();
    // Where the camera was left in it, or looking at the car from behind and to its left, the jump
    // ahead of it.
    if (auto* editorCam = dynamic_cast<EditorCameraController*>(cam.get()); editorCam && !LoadEditorCamera())
    {
        const glm::vec3 eye(-5.5f, 3.2f, -8.0f), target(0.0f, 0.6f, 3.0f);
        const glm::vec3 d = glm::normalize(target - eye);
        editorCam->SetPosition(eye);
        editorCam->SetYawPitch(std::atan2(d.z, d.x), std::asin(d.y));
    }
    Logger::Info("OnStart done");
}

void EditorApp::ClearScene()
{
    if (gameClock.IsPlaying())
        TogglePlay();
    SaveEditorCamera();
    scene.Clear();
    sceneRenderer.InvalidateTransforms();
    physics->Reset();
    scripts->Reset();
    undo.Clear();
    selectedEntityID = static_cast<uint64_t>(-1);
    selectedEntity = Entity{};
    if (inspectorWindow)
        inspectorWindow->ClearInspectedEntity();
    assets.UnloadUnused();
}

void EditorApp::NewScene()
{
    ClearScene();
    scenePath.clear();
    signaturePending = true;
    sceneChanged = false;
    signatureTimer = 0.0f;
}

bool EditorApp::OpenScene(const fs::path& file)
{
    if (file.empty())
        return false;
    ClearScene();
    std::string error, warnings;
    SceneSettings settings;
    if (!SceneFile::Load(scene, assets, file, error, &warnings, &settings))
    {
        Logger::Error(error);
        sceneStatus = error;
        return false;
    }
    scene.Update(0.0f);
    // The scene's light and the pipeline it is seen with (older files have neither).
    if (settings.hasLighting)
        sceneRenderer.lighting = settings.lighting;
    if (pipelineWindow && !settings.pipeline.empty() && settings.pipeline != pipelineWindow->PipelinePath())
        pipelineWindow->LoadPipeline(settings.pipeline);
    scenePath = file;
    // Taken after the next frame's update (Animators bind their graphs then).
    signaturePending = true;
    sceneChanged = false;
    signatureTimer = 0.0f;
    sceneStatus = warnings;
    RememberScene(file);
    LoadEditorCamera();
    return true;
}

SceneSettings EditorApp::CurrentSettings() const
{
    SceneSettings settings;
    settings.lighting = sceneRenderer.lighting;
    settings.hasLighting = true;
    settings.pipeline = pipelineWindow ? pipelineWindow->PipelinePath() : kDefaultPipeline;
    return settings;
}

bool EditorApp::SaveModifiedAssets(std::vector<fs::path>& written)
{
    bool saved = true;
    auto report = [&](bool ok, const fs::path& file, const std::string& error) {
        if (!error.empty())
            (ok ? Logger::Warning(error) : Logger::Error(error));
        if (ok)
        {
            written.push_back(file);
            OfferDownload(file);
        }
        saved = saved && ok;
    };

    // Materials kept in .material files.
    std::vector<std::pair<std::shared_ptr<MaterialAsset>, fs::path>> materials;
    assets.ForEach<MaterialAsset>([&](AssetID id, MaterialAsset& material) {
        if (const AssetMetadata* meta = assets.GetMetadata(id); meta && material.IsModified())
            if (fs::path file = MaterialFileOf(*meta); !file.empty())
                materials.emplace_back(assets.Get<MaterialAsset>(id), file);
    });
    for (const auto& [material, file] : materials)
    {
        std::string error;
        report(MaterialFile::Save(*material, assets, file, error), file, error);
    }

    // Animation graphs.
    std::vector<std::pair<std::shared_ptr<AnimationGraph>, fs::path>> graphs;
    assets.ForEach<AnimationGraph>([&](AssetID id, AnimationGraph& graph) {
        if (const AssetMetadata* meta = assets.GetMetadata(id); meta && graph.IsModified() && meta->path.extension() == ".animgraph")
            graphs.emplace_back(assets.Get<AnimationGraph>(id), meta->path);
    });
    for (const auto& [graph, file] : graphs)
    {
        std::string error;
        report(AnimationGraphFile::Save(*graph, assets, file, error), file, error);
    }

    // The render pipeline.
    if (pipelineWindow && pipelineWindow->IsModified())
    {
        const bool ok = pipelineWindow->SavePipeline(pipelineWindow->PipelinePath());
        report(ok, FileSystem::Resolve(pipelineWindow->PipelinePath()), std::string());
    }
    return saved;
}

bool EditorApp::SaveScene(const fs::path& file)
{
    std::vector<fs::path> written;
    SaveModifiedAssets(written); // what fails is in the log; the scene is saved anyway

    std::string error;
    const SceneSettings settings = CurrentSettings();
    if (!SceneFile::Save(scene, assets, file, error, &settings))
    {
        Logger::Error(error);
        sceneStatus = error;
        return false;
    }
    scenePath = file;
    signaturePending = true;
    sceneChanged = false;
    signatureTimer = 0.0f;
    sceneStatus.clear();
    RememberScene(file);
    SaveEditorCamera();
    std::string also;
    for (const fs::path& f : written)
        also += (also.empty() ? " (and " : ", ") + f.filename().string();
    Logger::Info("Saved " + file.string() + (also.empty() ? "" : also + ")"));
    UpdateModified();
    if (afterSavePrompt)
    {
        const std::function<void()> then = std::move(afterSavePrompt);
        afterSavePrompt = nullptr;
        then();
    }
    return true;
}

void EditorApp::SaveSceneFromMenu(bool saveAs)
{
    if (gameClock.IsPlaying())
        return; // what is saved is the scene as edited, not as the game left it
    if (saveAs || scenePath.empty())
    {
        saveScenePopup = true;
        return;
    }
    if (SaveScene(scenePath))
        OfferDownload(scenePath);
}

void EditorApp::OfferDownload(const fs::path& file)
{
#if defined(__EMSCRIPTEN__)
    EM_ASM({
        const data = FS.readFile(UTF8ToString($0));
        const url = URL.createObjectURL(new Blob([data], { type: 'application/json' }));
        const link = document.createElement('a');
        link.href = url;
        link.download = UTF8ToString($1);
        link.click();
        setTimeout(() => URL.revokeObjectURL(url), 1000);
    }, file.string().c_str(), file.filename().string().c_str());
#else
    (void)file;
#endif
}

void EditorApp::RememberScene(const fs::path& file)
{
    recentScenes.erase(std::remove(recentScenes.begin(), recentScenes.end(), file), recentScenes.end());
    recentScenes.insert(recentScenes.begin(), file);
    if (recentScenes.size() > 8)
        recentScenes.resize(8);
    // The project opens in it next time.
    const std::string resource = FileSystem::ToResourcePath(file);
    std::error_code ec;
    fs::create_directories(LastSceneFile().parent_path(), ec);
    std::ofstream(LastSceneFile(), std::ios::binary) << (resource.empty() ? file.generic_string() : resource);
}

fs::path EditorApp::LastSceneFile() const
{
    return project / ".mariana" / "editor" / "last-scene.txt";
}

void EditorApp::OpenStartScene()
{
    std::error_code ec;
    // The scene the project was left in.
    if (std::ifstream stream(LastSceneFile(), std::ios::binary); stream)
    {
        std::string last;
        std::getline(stream, last);
        const fs::path file = last.rfind('/', 0) == 0 ? FileSystem::Resolve(last) : fs::path(last);
        if (!file.empty() && fs::exists(file, ec) && OpenScene(file))
            return;
    }
    // Its playground; the Sandbox's is made in code when its file is missing.
    if (const fs::path playground = project / "Scenes" / "Playground.scene"; fs::exists(playground, ec) && OpenScene(playground))
        return;
    if (fs::equivalent(project, MARIANA_PROJECT_DIR, ec))
    {
        BuildStartupScene();
        return;
    }
    // Its first scene, else an empty one.
    std::vector<fs::path> scenes;
    for (const auto& entry : fs::directory_iterator(project / "Scenes", ec))
        if (entry.path().extension() == ".scene")
            scenes.push_back(entry.path());
    std::sort(scenes.begin(), scenes.end());
    for (const fs::path& file : scenes)
        if (OpenScene(file))
            return;
    NewScene();
}

bool EditorApp::SceneModified() const
{
    return sceneChanged || !unsavedFiles.empty();
}

void EditorApp::UpdateModified()
{
    // Asset files with edits.
    unsavedFiles.clear();
    assets.ForEach<MaterialAsset>([&](AssetID id, MaterialAsset& material) {
        if (const AssetMetadata* meta = assets.GetMetadata(id); meta && material.IsModified())
            if (fs::path file = MaterialFileOf(*meta); !file.empty())
                unsavedFiles.push_back(file);
    });
    assets.ForEach<AnimationGraph>([&](AssetID id, AnimationGraph& graph) {
        if (const AssetMetadata* meta = assets.GetMetadata(id); meta && graph.IsModified() && meta->path.extension() == ".animgraph")
            unsavedFiles.push_back(meta->path);
    });
    if (pipelineWindow && pipelineWindow->IsModified())
        unsavedFiles.push_back(FileSystem::Resolve(pipelineWindow->PipelinePath()));

    // The scene as it would be saved (while playing it is the one Stop puts back: unchanged).
    if (!gameClock.IsPlaying())
    {
        const SceneSettings settings = CurrentSettings();
        const uint64_t signature = SceneFile::Signature(scene, assets, &settings);
        if (signaturePending)
        {
            savedSignature = signature;
            signaturePending = false;
        }
        sceneChanged = signature != savedSignature;
    }
#if defined(__EMSCRIPTEN__)
    EM_ASM({ Module.marianaUnsaved = $0; }, SceneModified() ? 1 : 0);
#endif
}

void EditorApp::AskToSave(std::function<void()> then)
{
    UpdateModified();
    if (!SceneModified())
    {
        then();
        return;
    }
    afterSavePrompt = std::move(then);
    savePromptPopup = true;
}

bool EditorApp::OnCloseRequested()
{
    UpdateModified();
    if (!SceneModified())
        return true;
    AskToSave([this]() { Quit(); });
    return false;
}

fs::path EditorApp::EditorCameraFile() const
{
    if (scenePath.empty())
        return {};
    return project / ".mariana" / "editor" / (scenePath.stem().string() + ".camera");
}

void EditorApp::SaveEditorCamera() const
{
    const auto* editorCam = dynamic_cast<const EditorCameraController*>(cam.get());
    const fs::path file = EditorCameraFile();
    if (!editorCam || file.empty())
        return;
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    const glm::vec3 p = editorCam->Position();
    std::ofstream(file) << p.x << ' ' << p.y << ' ' << p.z << ' ' << editorCam->Yaw() << ' ' << editorCam->Pitch() << '\n';
}

bool EditorApp::LoadEditorCamera()
{
    auto* editorCam = dynamic_cast<EditorCameraController*>(cam.get());
    const fs::path file = EditorCameraFile();
    if (!editorCam || file.empty())
        return false;
    std::ifstream stream(file);
    glm::vec3 p;
    float yaw = 0.0f, pitch = 0.0f;
    if (!(stream >> p.x >> p.y >> p.z >> yaw >> pitch))
        return false;
    editorCam->SetPosition(p);
    editorCam->SetYawPitch(yaw, pitch);
    return true;
}

void EditorApp::DrawScenePopups()
{
    if (openScenePopup)
    {
        ImGui::OpenPopup("Open Scene");
        openScenePopup = false;
    }
    if (saveScenePopup)
    {
        ImGui::OpenPopup("Save Scene As");
        saveScenePopup = false;
    }
    if (savePromptPopup)
    {
        ImGui::OpenPopup("Save Changes?");
        savePromptPopup = false;
    }

    if (ImGui::BeginPopupModal("Save Changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const std::string name = scenePath.empty() ? std::string("Untitled") : scenePath.filename().string();
        ImGui::Text("Save the changes before going on?");
        if (sceneChanged)
            ImGui::BulletText("%s", name.c_str());
        for (const fs::path& file : unsavedFiles)
            ImGui::BulletText("%s", file.filename().string().c_str());
        ImGui::Spacing();
        if (ImGui::Button("Save", ImVec2(110.0f, 0.0f)))
        {
            ImGui::CloseCurrentPopup();
            if (gameClock.IsPlaying())
                TogglePlay(); // what is saved is the scene as edited
            if (scenePath.empty())
                saveScenePopup = true; // Save As runs what waited once it has saved
            else if (SaveScene(scenePath))
                OfferDownload(scenePath);
            else
                afterSavePrompt = nullptr;
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save", ImVec2(110.0f, 0.0f)))
        {
            ImGui::CloseCurrentPopup();
            const std::function<void()> then = std::move(afterSavePrompt);
            afterSavePrompt = nullptr;
            if (then)
                then();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(110.0f, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
            afterSavePrompt = nullptr;
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("Open Scene"))
    {
        // Every /Scenes/*.scene across the roots, the first root's when two have the same.
        std::vector<std::pair<std::string, fs::path>> found;
        for (const fs::path& root : FileSystem::Roots())
        {
            std::error_code ec;
            for (const auto& entry : fs::directory_iterator(root / "Scenes", ec))
            {
                if (entry.path().extension() != ".scene")
                    continue;
                const std::string name = entry.path().filename().string();
                if (std::none_of(found.begin(), found.end(), [&](const auto& f) { return f.first == name; }))
                    found.emplace_back(name, entry.path());
            }
        }
        if (found.empty())
            ImGui::TextDisabled("No scenes in /Scenes yet");
        for (const auto& [name, path] : found)
        {
            if (ImGui::Selectable(name.c_str(), path == scenePath))
            {
                AskToSave([this, path = path]() { OpenScene(path); });
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("Save Scene As"))
    {
        static char name[64] = {};
        if (ImGui::IsWindowAppearing())
        {
            const std::string current = scenePath.empty() ? std::string("New Scene") : scenePath.stem().string();
            std::snprintf(name, sizeof(name), "%s", current.c_str());
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::TextUnformatted("Save to the project's Scenes folder as");
        ImGui::SetNextItemWidth(220.0f);
        const bool entered = ImGui::InputText("##name", name, sizeof(name), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        ImGui::BeginDisabled(name[0] == '\0');
        if ((ImGui::Button("Save") || entered) && name[0] != '\0')
        {
            std::string clean = name;
            for (char& c : clean)
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_' && c != ' ')
                    c = '_';
            if (const fs::path file = project / "Scenes" / (clean + ".scene"); SaveScene(file))
                OfferDownload(file);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }

    // Save As closed without saving: what waited for the save doesn't happen.
    if (afterSavePrompt && !saveScenePopup && !savePromptPopup && !ImGui::IsPopupOpen("Save Scene As") && !ImGui::IsPopupOpen("Save Changes?"))
        afterSavePrompt = nullptr;
}

namespace
{
    std::string Utf8(const fs::path& path)
    {
        const std::u8string text = path.u8string();
        return std::string(text.begin(), text.end());
    }
}

void EditorApp::SwitchProject(const fs::path& folder)
{
    AskToSave([this, folder]() {
        // A new editor on the project, then this one closes (it saves its editor camera and layout).
        std::string error;
        if (!Process::Launch({ Utf8(FileSystem::ExecutablePath()), "--project", Utf8(folder) }, FileSystem::ExecutableDirectory(), error))
        {
            projectError = error;
            Logger::Error(error);
            return;
        }
        Project::Remember(folder);
        Quit();
    });
}

void EditorApp::DrawProjectPopups()
{
    if (newProjectPopup)
    {
        ImGui::OpenPopup("New Project");
        newProjectPopup = false;
        projectError.clear();
    }
    if (openProjectPopup)
    {
        ImGui::OpenPopup("Open Project");
        openProjectPopup = false;
        projectError.clear();
    }

    if (ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static char name[128] = {};
        static char location[1024] = {};
        if (ImGui::IsWindowAppearing())
        {
            std::snprintf(name, sizeof(name), "%s", "My Game");
            std::snprintf(location, sizeof(location), "%s", Utf8(project.parent_path()).c_str());
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::TextUnformatted("A new project: a folder for a game's scenes, models, textures and scripts.");
        ImGui::Spacing();
        ImGui::SetNextItemWidth(420.0f);
        ImGui::InputText("Name", name, sizeof(name));
        ImGui::SetNextItemWidth(420.0f);
        ImGui::InputText("Location", location, sizeof(location));
        const std::string text = name;
        const bool validName = !text.empty() && text.find_first_of("<>:\"/\\|?*") == std::string::npos && text != "." && text != "..";
        const fs::path folder = fs::path(std::u8string(location, location + std::strlen(location))) / fs::path(std::u8string(text.begin(), text.end()));
        ImGui::TextDisabled("%s", validName ? Utf8(folder.lexically_normal()).c_str() : "A name can't have < > : \" / \\ | ? * in it");
        ImGui::TextDisabled("It gets Scenes, Models, Textures, Materials and Scripts folders (with the vehicle and");
        ImGui::TextDisabled("character driver scripts) and a .gitignore. The editor then opens it in a new window.");
        if (!projectError.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f), "%s", projectError.c_str());
        ImGui::Spacing();
        ImGui::BeginDisabled(!validName || location[0] == '\0');
        if (ImGui::Button("Create", ImVec2(110.0f, 0.0f)))
        {
            // The driver scripts come from the Sandbox (the project the editor was built with).
            if (Project::Create(folder.lexically_normal(), fs::path(MARIANA_PROJECT_DIR) / "Scripts", projectError))
            {
                ImGui::CloseCurrentPopup();
                SwitchProject(folder.lexically_normal());
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(110.0f, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Open Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static char path[1024] = {};
        if (ImGui::IsWindowAppearing())
            path[0] = '\0';
        ImGui::TextUnformatted("A project folder (any folder: its files are the project's)");
        ImGui::SetNextItemWidth(520.0f);
        const bool entered = ImGui::InputTextWithHint("##folder", "C:/Games/My Game", path, sizeof(path), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        ImGui::BeginDisabled(path[0] == '\0');
        if (ImGui::Button("Open") || (entered && path[0] != '\0'))
        {
            std::error_code ec;
            const fs::path folder = fs::path(std::u8string(path, path + std::strlen(path))).lexically_normal();
            if (!fs::is_directory(folder, ec))
                projectError = "There is no folder " + Utf8(folder);
            else
            {
                ImGui::CloseCurrentPopup();
                SwitchProject(folder);
            }
        }
        ImGui::EndDisabled();
        ImGui::Spacing();
        ImGui::SeparatorText("Recent projects");
        const std::vector<fs::path> recent = Project::Recent();
        if (recent.empty())
            ImGui::TextDisabled("None yet");
        for (const fs::path& folder : recent)
        {
            std::error_code ec;
            const bool current = fs::equivalent(folder, project, ec);
            ImGui::PushID(Utf8(folder).c_str());
            if (ImGui::Selectable(Utf8(folder.filename()).c_str(), current, current ? ImGuiSelectableFlags_Disabled : 0, ImVec2(160.0f, 0.0f)))
            {
                ImGui::CloseCurrentPopup();
                SwitchProject(folder);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s%s", Utf8(folder).c_str(), current ? "  (open)" : "");
            ImGui::PopID();
        }
        if (!projectError.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f), "%s", projectError.c_str());
        ImGui::Spacing();
        if (ImGui::Button("Cancel", ImVec2(110.0f, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void EditorApp::BuildStartupScene()
{
    // What the Sandbox's Playground.scene was saved from: the vehicle playground with the concept car
    // (or the primitive car when the project doesn't have it) and a chase camera for the Game view,
    // three dancers in the car's way (thrown when it hits them; their heads turn to watch it come:
    // Dancer.animgraph's Look At, aimed by WatchCar.luau), a pool to its right with jellies on
    // the way there, a flag on its left, and scripts: R puts the car back at the start
    // (Respawn.luau) and a launch pad ahead on the left throws what drives onto it into the air
    // (LaunchPad.luau) under a hovering marker (Hover.luau).
    VehiclePresets::SpawnPlayground(scene, assets, glm::vec3(0.0f));
    scene.Update(0.0f);
    Entity car = VehiclePresets::SpawnCar(scene, assets, "/Models/CarConcept/CarConcept.gltf", glm::vec3(0.0f));
    // The keys drive it (the concept car with less steering lock the faster it goes).
    std::vector<ScriptValue> driver = { ScriptValue{ "steerFade", ScriptValueType::Number, 25.0f } };
    if (!car.IsValid())
    {
        car = VehiclePresets::Spawn(scene, assets, VehicleType::Wheeled, glm::vec3(0.0f, 1.0f, 0.0f));
        driver.clear();
    }
    ScriptWorld::AddScript(car, assets, "/Scripts/VehicleDriver.luau", driver);
    AddChaseCamera(7.0f, 2.5f, 1.0f);
    const glm::quat facingCar = glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    const AssetHandle<AnimationGraph> dance = assets.Load<AnimationGraph>(FileSystem::Resolve("/Animations/Dancer.animgraph").string());
    for (const glm::vec3& at : { glm::vec3(0.0f, 0.0f, 14.0f), glm::vec3(-0.9f, 0.0f, 16.0f), glm::vec3(0.9f, 0.0f, 16.0f) })
        if (Entity dancer = CharacterPresets::SpawnRagdoll(scene, assets, at, facingCar); dancer.IsValid())
        {
            Ragdoll* ragdoll = dancer.Get<Ragdoll>();
            ragdoll->knockback = 1.6f;
            ragdoll->knockbackLift = 0.6f;
            if (dance.Get())
            {
                dancer.Get<Animator>()->graph = dance;
                ScriptWorld::AddScript(dancer, assets, "/Scripts/WatchCar.luau");
            }
        }
    WaterPresets::SpawnPool(scene, assets, glm::vec3(15.0f, 0.0f, 4.0f));
    SoftBodyPresets::SpawnJelly(scene, assets, glm::vec3(3.5f, 0.0f, 6.0f));
    SoftBodyPresets::SpawnJelly(scene, assets, glm::vec3(5.0f, 0.0f, 4.5f));
    SoftBodyPresets::SpawnFlag(scene, assets, glm::vec3(-5.0f, 0.0f, 8.0f), facingCar);

    ScriptWorld::AddScript(car, assets, "/Scripts/Respawn.luau");
    Entity pad = scene.Instantiate("Launch Pad");
    pad.SetPosition(-10.0f, 0.0f, 20.0f);
    BoxCollider zone;
    zone.size = glm::vec3(3.0f, 1.5f, 3.0f);
    zone.center = glm::vec3(0.0f, 0.75f, 0.0f);
    zone.isTrigger = true;
    pad.Add<BoxCollider>(zone);
    ScriptWorld::AddScript(pad, assets, "/Scripts/LaunchPad.luau");
    Entity plate = BuiltinAssets::InstantiatePrimitive(scene, assets, BuiltinMesh::Cube, "Plate", glm::vec3(0.95f, 0.5f, 0.1f));
    plate.SetParent(pad);
    plate.SetPosition(0.0f, 0.01f, 0.0f);
    plate.SetScale(3.0f, 0.02f, 3.0f);
    Entity marker = BuiltinAssets::InstantiatePrimitive(scene, assets, BuiltinMesh::Cube, "Marker", glm::vec3(1.0f, 0.75f, 0.2f));
    marker.SetParent(pad);
    marker.SetPosition(0.0f, 3.0f, 0.0f);
    marker.SetScaleUniform(0.5f);
    const glm::quat tilt = glm::quat(glm::radians(glm::vec3(45.0f, 0.0f, 35.0f)));
    marker.SetRotationQuat(tilt.x, tilt.y, tilt.z, tilt.w);
    ScriptWorld::AddScript(marker, assets, "/Scripts/Hover.luau");

    scene.Update(0.0f);
    DeselectEntity();
    signaturePending = true;
    sceneChanged = false;
    signatureTimer = 0.0f;
}

void EditorApp::OnUpdate(float deltaTime)
{
    {
        MARIANA_PROFILE("Thumbnails");
        thumbnails->Update();
    }

    // The views take the sizes their windows had last frame, before this frame's UI shows them.
    if (sceneView->Resize(sceneWindow->PixelSize()))
        cam->SetViewportSize(sceneView->Size().x, sceneView->Size().y);
    if (!gameView && gameWindow->IsVisible())
        gameView = std::make_unique<ViewRenderer>(sceneRenderer, false);
    if (gameView)
        gameView->Resize(gameWindow->PixelSize());
    // The editor camera only answers to the mouse over the Scene window.
    if (auto* editorCam = dynamic_cast<EditorCameraController*>(cam.get()))
        editorCam->SetHovered(sceneWindow->IsHovered());

    if (!ImGui::GetIO().WantTextInput)
    {
        if (input.IsKeyPressed(Key::KEY_1))
            CurrentGizmoOperation = ImGuizmo::TRANSLATE;
        if (input.IsKeyPressed(Key::KEY_2))
            CurrentGizmoOperation = ImGuizmo::ROTATE;
        if (input.IsKeyPressed(Key::KEY_3))
            CurrentGizmoOperation = ImGuizmo::SCALE;
    }
    
    if(input.IsKeyPressed(Key::F11))
    {
        m_Window.ToggleFullscreen();
        OnWindowResized(static_cast<int>(m_Window.GetWidth()), static_cast<int>(m_Window.GetHeight()));
    }

    if (selectedEntityID != static_cast<uint64_t>(-1) &&
        input.IsKeyPressed(Key::DELETE) &&
        !ImGui::GetIO().WantTextInput &&
        !ImGuizmo::IsUsing())
    {
        scene.Destroy(selectedEntity);
        selectedEntity = Entity{};
        selectedEntityID = static_cast<uint64_t>(-1);
        if (inspectorWindow)
            inspectorWindow->ClearInspectedEntity();

        if (size_t unloaded = assets.UnloadUnused())
            Logger::Info("Unloaded " + std::to_string(unloaded) + " unused assets");
    }
    
    // A click in the Scene window asked for the id under it (DrawSceneOverlay); it arrives a frame
    // or two later.
    uint64_t pickedID;
    if (sceneView->Graph().GetTexture(SceneView::kEntityIds).TryGetPixelReadback(pickedID))
    {
        // 0: nothing was drawn there (the prepass clears to 0, and flecs never uses id 0).
        DeselectEntity();
        if (pickedID != 0 && pickedID != UINT64_MAX)
            SelectEntity(pickedID);
    }

    // Script files edited elsewhere take effect at once (also while playing).
    scripts->ReloadChanged();

    // Game time: scripts, animations and physics only move while the game plays (scripts on the
    // frames it moves: not while paused, but on a Step). Transforms changed here are propagated
    // once, in OnRender. The keys drive the game unless something else has them (typing, flying the
    // editor camera).
    const bool gameHasKeys = !ImGui::GetIO().WantTextInput && !input.IsMouseButtonDown(MouseButton::Right);
    const bool gameAdvances = gameClock.IsPlaying() && (!gameClock.IsPaused() || gameClock.DeltaTime() > 0.0f);
    if (gameAdvances)
        scripts->Update(gameClock.DeltaTime(), gameHasKeys ? &input : nullptr);
    {
        MARIANA_PROFILE("Animation");
        animationSystem.Run(gameClock.DeltaTime());
    }
    if (gameClock.IsPlaying())
    {
        physics->Step(gameClock.DeltaTime());
        // Paused, a Step shows what its step did rather than the frame before it.
        if (gameClock.IsPaused())
            physics->ShowLatest();
        if (gameAdvances)
            scripts->LateUpdate(gameClock.DeltaTime());
    }

    sceneCameras.clear();
    cameraSystem.Run();

    signatureTimer -= deltaTime;
    if (signatureTimer <= 0.0f)
    {
        signatureTimer = 0.5f;
        UpdateModified();
    }
}

void EditorApp::OnGUI()
{
    gui.PreUpdateGUI();
    ImGuizmo::BeginFrame();
    graphWindow->SetPreview(selectedEntity);

    // Global, so a focused text field keeps Ctrl+Z for itself.
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal))
        undo.Undo();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal) ||
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal))
        undo.Redo();
    // Unity's play shortcuts.
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_P, ImGuiInputFlags_RouteGlobal))
        TogglePlay();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P, ImGuiInputFlags_RouteGlobal))
        TogglePause();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiKey_P, ImGuiInputFlags_RouteGlobal))
        gameClock.Step();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
        SaveSceneFromMenu(false);
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
        SaveSceneFromMenu(true);
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal))
        openScenePopup = true;
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N, ImGuiInputFlags_RouteGlobal))
        AskToSave([this]() { NewScene(); });
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_B, ImGuiInputFlags_RouteGlobal))
        buildWindow->Build(false);
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_B, ImGuiInputFlags_RouteGlobal))
        buildWindow->Build(true);

    DrawTopMenu();
    // The views in a fixed order, Scene last: the first time they dock, it ends up the open tab.
    for (auto& [name, window] : EditorWindows)
    {
        if (window.get() != sceneWindow && window.get() != gameWindow)
        {
            MARIANA_PROFILE_NAME(name + " window");
            window->Draw();
        }
    }
    gameWindow->Draw();
    sceneWindow->Draw();
}

void EditorApp::DrawSceneOverlay(ViewportWindow& window)
{
    DrawCameraFrustums(window);
    PhysicsEditor::DrawColliders(scene, selectedEntity, physicsWindow->showAllColliders, cam->Projection() * cam->View(),
                                 window.ImageMin(), window.ImageMax() - window.ImageMin(), physics.get());
    // Jolt's own view of the simulation, while there is one.
    PhysicsDebugDraw debugDraw = physicsWindow->debugDrawSettings;
    if (!physicsWindow->debugDraw || !gameClock.IsPlaying())
        debugDraw.shapes = debugDraw.contacts = debugDraw.velocities = debugDraw.centerOfMass = debugDraw.boundingBoxes = false;
    debugDraw.cameraPosition = cam->GetCameraInfo().pos;
    debugLines.clear();
    physics->DebugDraw(debugDraw, debugLines);
    physicsWindow->debugLines = debugLines.size();
    PhysicsEditor::DrawDebugLines(debugLines, cam->Projection() * cam->View(), window.ImageMin(), window.ImageMax() - window.ImageMin());

    // Collider handles come before the gizmo: a click on one is theirs.
    const bool handleHit = selectedEntityID != static_cast<uint64_t>(-1) &&
        PhysicsEditor::DrawColliderHandles(selectedEntity, undo, cam->Projection() * cam->View(), window.ImageMin(), window.ImageMax() - window.ImageMin());
    bool gizmoHit = handleHit;
    ImGuizmo::Enable(!handleHit);
    if (selectedEntityID != static_cast<uint64_t>(-1) && selectedEntity.Has<WorldXform>())
    {
        WorldXform& form = *selectedEntity.Get<WorldXform>();
        glm::mat4 world = glm::make_mat4(form.model);
        const LocalTRS before = selectedEntity.Has<LocalTRS>() ? *selectedEntity.TryGet<LocalTRS>() : LocalTRS{};

        const bool manipulated = DrawGizmo(world, cam->View(), cam->Projection(), window);
        gizmoHit |= ImGuizmo::IsOver() || ImGuizmo::IsUsing();
        if (manipulated)
        {
            glm::mat4 local = world;
            Entity parent = scene.Parent(selectedEntity);
            if (parent.IsValid()) {
                glm::mat4 parentWorld = glm::make_mat4(parent.Get<WorldXform>()->model);
                local = glm::inverse(parentWorld) * world;
            }

            glm::vec3 translation = glm::vec3(local[3]);
            glm::vec3 scale(
                glm::length(glm::vec3(local[0])),
                glm::length(glm::vec3(local[1])),
                glm::length(glm::vec3(local[2])));

            glm::mat3 rotationMatrix(1.0f);
            if (scale.x > 0.000001f) rotationMatrix[0] = glm::vec3(local[0]) / scale.x;
            if (scale.y > 0.000001f) rotationMatrix[1] = glm::vec3(local[1]) / scale.y;
            if (scale.z > 0.000001f) rotationMatrix[2] = glm::vec3(local[2]) / scale.z;

            glm::quat rotation = glm::normalize(glm::quat_cast(rotationMatrix));

            selectedEntity.SetPosition(translation.x, translation.y, translation.z);
            selectedEntity.SetRotationQuat(rotation.x, rotation.y, rotation.z, rotation.w);
            selectedEntity.SetScale(scale.x, scale.y, scale.z);
        }
        // A drag is one undo step, recorded when it is let go.
        if (selectedEntity.Has<LocalTRS>())
            gizmoUndo.Track(selectedEntity, before, ImGuizmo::IsUsing(), undo, CurrentGizmoOperation == ImGuizmo::ROTATE ? "Rotate" : CurrentGizmoOperation == ImGuizmo::SCALE ? "Scale" : "Move");
    }
    ImGuizmo::Enable(true);

    // A click that misses the gizmo picks what is under it (SceneView::kEntityIds; OnUpdate takes the answer).
    if (window.IsHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !gizmoHit)
    {
        const ImVec2 mouse = ImGui::GetMousePos();
        const glm::ivec2 pixel = window.ToPixel(glm::vec2(mouse.x, mouse.y), sceneView->Size());
        if (pixel.x >= 0 && pixel.y >= 0 && pixel.x < static_cast<int>(sceneView->Size().x) && pixel.y < static_cast<int>(sceneView->Size().y))
            sceneView->Graph().GetTexture(SceneView::kEntityIds).RequestPixelReadback(pixel.x, pixel.y);
    }
}

void EditorApp::DrawCameraFrustums(const ViewportWindow& window)
{
    // Each camera as a short pyramid along its view (the Game window's aspect), the one it shows
    // brighter and the selected one highlighted.
    const glm::vec2 min = window.ImageMin();
    const glm::vec2 extent = window.ImageMax() - min;
    const glm::mat4 viewProj = cam->Projection() * cam->View();
    const float aspect = gameView && gameView->Size().y > 0 ? static_cast<float>(gameView->Size().x) / gameView->Size().y : 16.0f / 9.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (size_t i = 0; i < sceneCameras.size(); ++i)
    {
        Entity entity = sceneCameras[i];
        const WorldXform* world = entity.TryGet<WorldXform>();
        const CameraComponent* camera = entity.TryGet<CameraComponent>();
        if (!world || !camera)
            continue;

        const glm::mat4 model = glm::make_mat4(world->model);
        const glm::vec3 origin(model[3]);
        const glm::vec3 right = glm::normalize(glm::vec3(model[0]));
        const glm::vec3 up = glm::normalize(glm::vec3(model[1]));
        const glm::vec3 forward = glm::normalize(glm::vec3(model[2]));
        const float depth = 1.0f;
        const float halfHeight = std::tan(glm::radians(camera->fov) * 0.5f) * depth;
        const float halfWidth = halfHeight * aspect;

        const glm::vec3 points[5] = {
            origin,
            origin + forward * depth - right * halfWidth + up * halfHeight,
            origin + forward * depth + right * halfWidth + up * halfHeight,
            origin + forward * depth + right * halfWidth - up * halfHeight,
            origin + forward * depth - right * halfWidth - up * halfHeight,
        };
        ImVec2 screen[5];
        bool visible = true;
        for (int p = 0; p < 5 && visible; ++p)
        {
            const glm::vec4 clip = viewProj * glm::vec4(points[p], 1.0f);
            visible = clip.w > 1e-3f;
            const glm::vec2 ndc = glm::vec2(clip) / clip.w;
            screen[p] = ImVec2(min.x + (ndc.x * 0.5f + 0.5f) * extent.x, min.y + (0.5f - ndc.y * 0.5f) * extent.y);
        }
        if (!visible)
            continue;

        const bool selected = selectedEntity.IsValid() && entity.RawId() == selectedEntity.RawId();
        const ImU32 color = selected ? IM_COL32(255, 200, 60, 255) : i == 0 ? IM_COL32(255, 255, 255, 220) : IM_COL32(200, 200, 200, 120);
        for (int p = 1; p <= 4; ++p)
        {
            drawList->AddLine(screen[0], screen[p], color, 1.5f);
            drawList->AddLine(screen[p], screen[p % 4 + 1], color, 1.5f);
        }
        // Which way is up.
        const glm::vec4 topClip = viewProj * glm::vec4(origin + forward * depth + up * halfHeight * 1.4f, 1.0f);
        if (topClip.w > 1e-3f)
        {
            const glm::vec2 ndc = glm::vec2(topClip) / topClip.w;
            const ImVec2 top(min.x + (ndc.x * 0.5f + 0.5f) * extent.x, min.y + (0.5f - ndc.y * 0.5f) * extent.y);
            drawList->AddTriangleFilled(screen[1], screen[2], top, color);
        }
    }
}

void EditorApp::SelectEntity(uint64_t id)
{
    if(ImGuizmo::IsOver() || ImGuizmo::IsUsing()) return;
    Entity entity = scene.FromId(id);
    if (!entity.IsValid())
        return;

    selectedEntityID = entity.RawId();
    selectedEntity = entity;
    if (inspectorWindow)
        inspectorWindow->SetInspectedEntity(scene, selectedEntity);
}

void EditorApp::DeselectEntity()
{
    if(ImGuizmo::IsOver() || ImGuizmo::IsUsing()) return;
    selectedEntityID = -1;
    if (inspectorWindow)
        inspectorWindow->ClearInspectedEntity();
}

#pragma region GUI


void EditorApp::DrawMat4(const char* id, float m[16], bool editable, float speed, const char* fmt)
{
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit
                          | ImGuiTableFlags_Borders
                          | ImGuiTableFlags_RowBg
                          | ImGuiTableFlags_NoSavedSettings;

    if (ImGui::BeginTable(id, 5, flags))
    {
        ImGui::TableSetupColumn(" ", ImGuiTableColumnFlags_WidthFixed, 22.0f);
        ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("W", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6, 4));

        for (int r = 0; r < 4; ++r)
        {
            ImGui::TableNextRow();

            // Row label
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("r%d", r);

            for (int c = 0; c < 4; ++c)
            {
                ImGui::TableSetColumnIndex(c + 1);

                // Highlight the diagonal
                bool is_diag = (r == c);
                if (is_diag)
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 220, 120, 255)); // warm tint

                ImGui::PushID(r * 4 + c);
                float& v = m[r * 4 + c]; // <-- change to m[c*4 + r] if your data is column-major

                if (editable)
                {
                    // Right-aligned cells
                    float w = ImGui::GetContentRegionAvail().x;
                    ImGui::SetNextItemWidth(w);
                    ImGui::DragFloat("##cell", &v, speed, 0, 0, fmt);
                }
                else
                {
                    ImGui::Text(fmt, v);
                }

                ImGui::PopID();
                if (is_diag)
                    ImGui::PopStyleColor();
            }
        }

        ImGui::PopStyleVar();
        ImGui::EndTable();

        // Small legend
        ImGui::TextDisabled("Diagonal highlighted • %s", editable ? "editable" : "read-only");
    }
}

void EditorApp::DrawTopMenu()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            const bool playing = gameClock.IsPlaying();
#if defined(__EMSCRIPTEN__)
            const bool canSwitchProjects = false;
#else
            const bool canSwitchProjects = true;
#endif
            if (ImGui::MenuItem("New Project...", nullptr, false, canSwitchProjects))
                newProjectPopup = true;
            if (ImGui::MenuItem("Open Project...", nullptr, false, canSwitchProjects))
                openProjectPopup = true;
            if (!canSwitchProjects && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("The web editor has its project built in: open other projects in the desktop editor");
            ImGui::Separator();
            if (ImGui::MenuItem("New Scene", "Ctrl+N"))
                AskToSave([this]() { NewScene(); });
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
                openScenePopup = true;
            if (ImGui::BeginMenu("Open Recent", !recentScenes.empty()))
            {
                for (const fs::path& recent : std::vector<fs::path>(recentScenes))
                    if (ImGui::MenuItem(recent.filename().string().c_str()))
                        AskToSave([this, recent]() { OpenScene(recent); });
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, !playing))
                SaveSceneFromMenu(false);
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S", false, !playing))
                SaveSceneFromMenu(true);
            if (playing && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Stop the game first: a scene is saved as edited");
            ImGui::Separator();
            if (ImGui::BeginMenu("Build"))
            {
                // What is built is what is saved: the scene and assets as their files have them.
                if (ImGui::MenuItem("Build Settings..."))
                    buildWindow->SetOpen(true);
                const bool canBuild = BuildWindow::CanBuild() && !buildWindow->IsBuilding();
                if (ImGui::MenuItem("Build", "Ctrl+B", false, canBuild))
                    buildWindow->Build(false);
                if (ImGui::MenuItem("Build and Run", "Ctrl+Shift+B", false, canBuild))
                    buildWindow->Build(true);
                if (!BuildWindow::CanBuild() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("Building runs CMake and Emscripten: it needs the desktop editor");
                if (SceneModified())
                    ImGui::TextDisabled("Unsaved changes aren't in a build: save first");
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Preferences...", "Ctrl+,")) {}
            ImGui::Separator();
#ifndef __EMSCRIPTEN__
            if (ImGui::MenuItem("Exit"))
                AskToSave([this]() { Quit(); });
#endif
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem(undo.CanUndo() ? ("Undo " + undo.UndoName()).c_str() : "Undo", "Ctrl+Z", false, undo.CanUndo()))
                undo.Undo();
            if (ImGui::MenuItem(undo.CanRedo() ? ("Redo " + undo.RedoName()).c_str() : "Redo", "Ctrl+Y", false, undo.CanRedo()))
                undo.Redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {/* TODO */}
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {/* TODO */}
            if (ImGui::MenuItem("Paste", "Ctrl+V")) {/* TODO */}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Empty Entity")) SpawnEmpty();
            ImGui::Separator();
            if (ImGui::MenuItem("Cube")) SpawnPrimitive(BuiltinMesh::Cube, "Cube");
            if (ImGui::MenuItem("Sphere")) SpawnPrimitive(BuiltinMesh::Sphere, "Sphere");
            if (ImGui::MenuItem("Plane")) SpawnPrimitive(BuiltinMesh::Plane, "Plane");
            if (ImGui::MenuItem("Cylinder")) SpawnPrimitive(BuiltinMesh::Cylinder, "Cylinder");
            if (ImGui::MenuItem("Capsule")) SpawnPrimitive(BuiltinMesh::Capsule, "Capsule");
            ImGui::Separator();
            if (ImGui::MenuItem("Camera")) SpawnCamera();
            ImGui::Separator();
            if (ImGui::BeginMenu("Physics"))
            {
                if (ImGui::MenuItem("Dynamic Cube")) SpawnPhysicsShape(BuiltinMesh::Cube, "Cube", true);
                if (ImGui::MenuItem("Dynamic Sphere")) SpawnPhysicsShape(BuiltinMesh::Sphere, "Sphere", true);
                if (ImGui::MenuItem("Dynamic Capsule")) SpawnPhysicsShape(BuiltinMesh::Capsule, "Capsule", true);
                if (ImGui::MenuItem("Dynamic Cylinder")) SpawnPhysicsShape(BuiltinMesh::Cylinder, "Cylinder", true);
                ImGui::Separator();
                if (ImGui::MenuItem("Static Cube")) SpawnPhysicsShape(BuiltinMesh::Cube, "Static Cube", false);
                if (ImGui::MenuItem("Ground")) SpawnGround();
                if (ImGui::MenuItem("Box Stack")) SpawnBoxStack();
                ImGui::Separator();
                if (ImGui::MenuItem("Character")) SpawnCharacter();
                if (ImGui::MenuItem("Character Course")) SpawnCharacterCourse();
                ImGui::Separator();
                if (ImGui::MenuItem("Chain")) SpawnJointPreset(0);
                if (ImGui::MenuItem("Door")) SpawnJointPreset(1);
                if (ImGui::MenuItem("Spinner")) SpawnJointPreset(2);
                if (ImGui::MenuItem("Wrecking Ball")) SpawnJointPreset(3);
                ImGui::Separator();
                if (ImGui::MenuItem("Water")) SpawnJointPreset(4);
                if (ImGui::MenuItem("Water Pool")) SpawnJointPreset(5);
                ImGui::Separator();
                if (ImGui::MenuItem("Ragdoll (dancing)")) SpawnJointPreset(6);
                if (ImGui::BeginMenu("Soft Body"))
                {
                    if (ImGui::MenuItem("Flag")) SpawnJointPreset(7);
                    if (ImGui::MenuItem("Cloth Drape")) SpawnJointPreset(8);
                    if (ImGui::MenuItem("Jelly Cube")) SpawnJointPreset(9);
                    if (ImGui::MenuItem("Balloon")) SpawnJointPreset(10);
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Vehicle"))
            {
                if (ImGui::MenuItem("Car")) SpawnVehicle(VehicleType::Wheeled);
                if (ImGui::MenuItem("Motorcycle")) SpawnVehicle(VehicleType::Motorcycle);
                if (ImGui::MenuItem("Tank")) SpawnVehicle(VehicleType::Tracked);
                ImGui::Separator();
                if (ImGui::MenuItem("Playground")) SpawnPlayground();
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            if (ImGui::MenuItem("Scene")) { sceneWindow->SetOpen(!sceneWindow->IsOpen()); }
            if (ImGui::MenuItem("Game")) { gameWindow->SetOpen(!gameWindow->IsOpen()); }
            if (ImGui::MenuItem("Assets")) { EditorWindows["Assets"]->SetOpen(!EditorWindows["Assets"]->IsOpen()); }
            if (ImGui::MenuItem("Hierarchy")) { EditorWindows["Hierarchy"]->SetOpen(!EditorWindows["Hierarchy"]->IsOpen()); }
            if (ImGui::MenuItem("Inspector")) { EditorWindows["Inspector"]->SetOpen(!EditorWindows["Inspector"]->IsOpen()); }
            if (ImGui::MenuItem("Stats")) { EditorWindows["Stats"]->SetOpen(!EditorWindows["Stats"]->IsOpen()); }
            if (ImGui::MenuItem("Profiler")) { EditorWindows["Profiler"]->SetOpen(!EditorWindows["Profiler"]->IsOpen()); }
            if (ImGui::MenuItem("Loaded Assets")) { EditorWindows["Loaded Assets"]->SetOpen(!EditorWindows["Loaded Assets"]->IsOpen()); }
            if (ImGui::MenuItem("Render Pipeline")) { EditorWindows["Render Pipeline"]->SetOpen(!EditorWindows["Render Pipeline"]->IsOpen()); }
            if (ImGui::MenuItem("Lighting")) { EditorWindows["Lighting"]->SetOpen(!EditorWindows["Lighting"]->IsOpen()); }
            if (ImGui::MenuItem("Animation Graph")) { graphWindow->SetOpen(!graphWindow->IsOpen()); }
            if (ImGui::MenuItem("Physics")) { physicsWindow->SetOpen(!physicsWindow->IsOpen()); }
            if (ImGui::MenuItem("Script Editor")) { scriptEditorWindow->SetOpen(!scriptEditorWindow->IsOpen()); }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("Documentation")) {/* open URL */}
            if (ImGui::MenuItem("Report Issue"))   {/* open URL */}
            ImGui::Separator();
            if (ImGui::MenuItem("About")) {}
            ImGui::EndMenu();
        }

        DrawPlayControls();

        // The scene's file on the right, with a * while it has changes.
        const std::string title = (scenePath.empty() ? std::string("Untitled") : scenePath.filename().string()) + (SceneModified() ? " *" : "");
        const float titleWidth = ImGui::CalcTextSize(title.c_str()).x;
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX() + 16.0f, ImGui::GetWindowWidth() - titleWidth - ImGui::GetStyle().ItemSpacing.x * 2.0f));
        ImGui::TextDisabled("%s", title.c_str());
        if (ImGui::IsItemHovered() && (SceneModified() || !sceneStatus.empty()))
        {
            ImGui::BeginTooltip();
            if (SceneModified())
            {
                ImGui::TextUnformatted("Not saved yet (Ctrl+S saves them all):");
                if (sceneChanged)
                    ImGui::BulletText("the scene");
                for (const fs::path& file : unsavedFiles)
                    ImGui::BulletText("%s", file.filename().string().c_str());
            }
            if (!sceneStatus.empty())
                ImGui::TextDisabled("%s", sceneStatus.c_str());
            ImGui::EndTooltip();
        }

        ImGui::EndMainMenuBar();
    }
    DrawScenePopups();
    DrawProjectPopups();
}

void EditorApp::DrawPlayControls()
{
    // Nerd Font glyphs, as UTF-8 (play U+F04B, stop U+F04D, pause U+F04C, step forward U+F051).
    const char* const kPlay = "\xef\x81\x8b";
    const char* const kStop = "\xef\x81\x8d";
    const char* const kPause = "\xef\x81\x8c";
    const char* const kStep = "\xef\x81\x91";

    const float buttonWidth = ImGui::GetFrameHeight() * 1.6f;
    const float width = buttonWidth * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), (ImGui::GetWindowWidth() - width) * 0.5f));

    // A button drawn pressed while its state is on.
    auto button = [&](const char* glyph, const char* id, bool on, const char* tooltip) {
        if (on)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        ImGui::PushID(id);
        const bool pressed = ImGui::Button(glyph, ImVec2(buttonWidth, 0.0f));
        ImGui::PopID();
        if (on)
            ImGui::PopStyleColor();
        ImGui::SetItemTooltip("%s", tooltip);
        return pressed;
    };

    const bool playing = gameClock.IsPlaying();
    if (button(playing ? kStop : kPlay, "Play", playing, playing ? "Stop (Ctrl+P)" : "Play (Ctrl+P)"))
        TogglePlay();
    ImGui::BeginDisabled(!playing);
    if (button(kPause, "Pause", gameClock.IsPaused(), gameClock.IsPaused() ? "Resume (Ctrl+Shift+P)" : "Pause (Ctrl+Shift+P)"))
        TogglePause();
    if (button(kStep, "Step", false, "Step one frame (Ctrl+Alt+P)"))
        gameClock.Step();
    ImGui::EndDisabled();
    if (playing)
        ImGui::TextDisabled("%.2f s", gameClock.Time());
}

void EditorApp::TogglePlay()
{
    if (gameClock.IsPlaying())
    {
        gameClock.Stop();
        // The scripts end (OnDestroy) in the scene as it was played, before it is put back.
        scripts->Reset();
        RestoreScene();
        physics->Reset();
    }
    else
    {
        // Bodies and scripts are made from the scene as it is now.
        physics->Reset();
        scripts->Reset();
        keepOnStop.clear();
        playSnapshot = scene.Snapshot();
        playUndo = undo;
        restoreOnStop = true;
        gameClock.Play();
    }
}

void EditorApp::RestoreScene()
{
    if (!restoreOnStop)
        return;
    restoreOnStop = false;

    // What the entities marked to keep their changes are like now (with their children, if asked).
    std::vector<std::pair<uint64_t, SceneFile::EntityState>> kept;
    std::string keptName;
    for (const auto& [id, children] : keepOnStop)
    {
        Entity e = scene.FromId(id);
        if (!e.IsValid())
            continue;
        keptName = keptName.empty() ? std::string(e.GetName() ? e.GetName() : "entity") : std::string("entities");
        kept.emplace_back(id, SceneFile::SaveEntity(e, assets));
        if (children)
            scene.ForEachDescendant(e, [&](Entity d) { kept.emplace_back(d.RawId(), SceneFile::SaveEntity(d, assets)); });
    }
    keepOnStop.clear();

    scene.Restore(playSnapshot);
    playSnapshot = SceneSnapshot();

    // They get those back (entities made while playing are gone), as an undo step.
    std::vector<std::pair<uint64_t, SceneFile::EntityState>> before;
    for (const auto& [id, state] : kept)
    {
        Entity e = scene.FromId(id);
        if (!e.IsValid())
            continue;
        before.emplace_back(id, SceneFile::SaveEntity(e, assets));
        SceneFile::LoadEntity(e, assets, state);
    }
    scene.Update(0.0f);
    sceneRenderer.InvalidateTransforms();

    // The selection may have been made while playing.
    if (!selectedEntity.IsValid())
    {
        selectedEntityID = -1;
        selectedEntity = Entity{};
        inspectorWindow->ClearInspectedEntity();
    }
    undo = std::move(playUndo);
    playUndo = UndoStack();
    if (!before.empty())
    {
        auto apply = [this](const std::vector<std::pair<uint64_t, SceneFile::EntityState>>& states) {
            for (const auto& [id, state] : states)
                if (Entity e = scene.FromId(id); e.IsValid())
                    SceneFile::LoadEntity(e, assets, state);
        };
        undo.Push("Keep play-mode changes of " + keptName,
                  [apply, before]() { apply(before); },
                  [apply, kept]() { apply(kept); });
        Logger::Info("Kept the play-mode changes of " + std::to_string(before.size()) + (before.size() == 1 ? " entity" : " entities"));
    }
    // What only the snapshot or the play session held on to.
    assets.UnloadUnused();
}

void EditorApp::TogglePause()
{
    if (gameClock.IsPaused())
        gameClock.Play();
    else
        gameClock.Pause();
}

bool EditorApp::DrawGizmo(glm::mat4& transform, const glm::mat4& view, const glm::mat4& proj, const ViewportWindow& window)
{
    ImGuizmo::SetOrthographic(false);
    // In the Scene window, over its image.
    ImGuizmo::SetDrawlist();
    const glm::vec2 min = window.ImageMin();
    const glm::vec2 size = window.ImageMax() - min;
    ImGuizmo::SetRect(min.x, min.y, size.x, size.y);

    float matrix[16];
    memcpy(matrix, glm::value_ptr(transform), sizeof(matrix));

    bool manipulated = ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                                            CurrentGizmoOperation, ImGuizmo::LOCAL, matrix);
    if (manipulated)
    {
        transform = glm::make_mat4(matrix);
    }

    return manipulated;
}


#pragma endregion

void EditorApp::OnRender()
{
    {
        MARIANA_PROFILE("Transforms");
        scene.Update(0.0f);
    }
    Profiler::Count("Entities", static_cast<double>(scene.EntityCount()));

    // Only the views whose windows show are drawn; the scene's data is uploaded once for both.
    const Entity gameCameraEntity = sceneCameras.empty() ? Entity{} : sceneCameras.front();
    const bool drawScene = sceneWindow->IsVisible() && sceneView->Image();
    const bool drawGame = gameWindow->IsVisible() && gameView && gameView->Image() && gameCameraEntity.IsValid();
    if (drawScene || drawGame)
        sceneRenderer.Update();
    if (drawScene)
        sceneView->Render(*cam);
    if (drawGame)
    {
        // As the game sees it: the camera's view with the editor's pipeline and exposure.
        gameView->MirrorEffects(sceneView->Effects());
        gameCamera.Follow(*gameCameraEntity.TryGet<WorldXform>(), *gameCameraEntity.TryGet<CameraComponent>());
        gameCamera.SetViewportSize(gameView->Size().x, gameView->Size().y);
        gameCamera.SetExposure(cam->Exposure());
        gameCamera.UpdateBuffer();
        gameView->Render(gameCamera);
    }
    Renderer::RenderOverlay([this](void* pass) { gui.PostUpdateGUI(static_cast<wgpu::RenderPassEncoder*>(pass)); });
}

void EditorApp::OnShutdown()
{
    Logger::Info("Shutdown now");
    SaveEditorCamera();
    gui.KillGui();
}

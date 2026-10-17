#include <Editor/EditorApp.hpp>
#include <Editor/ThumbnailCache.hpp>
#include <Mariana/Foundation/FileReader.hpp>
#include <Editor/EditorCameraController.hpp>
#include <Mariana/Importers/GLTFLoader.hpp>
#include <Mariana/Importers/MaterialFile.hpp>
#include <Mariana/Rendering/BuiltinAssets.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>
#include <Mariana/Rendering/RenderPipeline.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Rendering/SceneView.hpp>
#include <Mariana/Scene/ECS.hpp>
#include <Mariana/Scene/Prefab.hpp>
#include <Mariana/Animation/AnimationPlayer.hpp>

#include "ImGuizmo.h"

#include <Editor/Windows/Stats.hpp>
#include <Editor/Windows/AssetsExplorer.hpp>
#include <Editor/Windows/HierarchyWindow.hpp>
#include <Editor/Windows/InspectorWindow.hpp>
#include <Editor/Windows/LoadedAssetsWindow.hpp>
#include <Editor/Windows/RenderPipelineWindow.hpp>
#include <Editor/Windows/LightingWindow.hpp>
#include <Editor/Windows/ViewportWindow.hpp>

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

EditorApp::EditorApp() : Application({ .title = "Mariana Engine Editor", .width = 1366, .height = 768, .iconPath = "/logo.png" }),
sceneRenderer(scene, assets),
gameCamera(input)
{
    AnimationPlayer::RegisterComponents();
    SceneRenderer::RegisterComponents();

    scene.UpdateComponentRegistry();

    cam = std::make_unique<EditorCameraController>(input);

    // The scene view's graph has the scene passes, ambient occlusion and the post-process chain
    // that ends in the image the Scene window shows (sized to it).
    sceneRenderer.Init();
    sceneView = std::make_unique<ViewRenderer>(sceneRenderer, true);
    GLTF::GLTFLoader::RegisterLoader(assets);
    MaterialFile::RegisterLoader(assets);

    // The effects, their settings, the exposure and the shadow settings come from a pipeline file.
    // The game view copies the scene view's.
    RenderPipelineDesc pipeline;
    std::string pipelineError;
    if (RenderPipelineDesc::Load(kDefaultPipeline, pipeline, pipelineError))
        pipeline.Apply(sceneView->Effects(), sceneRenderer, cam.get());
    else
        Logger::Error("Could not load the render pipeline: " + pipelineError);

    animationSystem = scene.CreateSystem<AnimationPlayer>([](Entity e, AnimationPlayer& player, float dt) {
        player.Update(e, dt);
    });
    cameraSystem = scene.CreateSystem<CameraComponent, WorldXform>([this](Entity e, CameraComponent&, WorldXform&, float) {
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
    AssetHandle<IMesh> mesh = BuiltinAssets::PrimitiveMesh(assets, shape);
    if (!mesh.IsValid())
        return {};

    // Each spawned shape gets its own material, so editing one leaves the others alone.
    AssetHandle<MaterialAsset> material = assets.RegisterAsset<MaterialAsset>(std::make_shared<MaterialAsset>(), AssetType::Material, std::string(name) + " Material");

    // A few metres in front of the camera.
    const CameraInfo info = cam->GetCameraInfo();
    const glm::vec3 position = info.pos + glm::normalize(glm::vec3(info.invView[2])) * 5.0f;

    Entity spawned = scene.Instantiate(name);
    spawned.SetPosition(position.x, position.y, position.z);
    spawned.Add<MeshComponent>(MeshComponent{ mesh, { material } }).AddTag<ShadowCasterTag>();
    scene.Update(0.0f);
    DeselectEntity();
    SelectEntity(spawned.RawId());
    return spawned;
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

    EditorWindows["Stats"] = std::make_unique<StatsWindow>(gui);
    thumbnails = std::make_unique<ThumbnailCache>(MARIANA_PROJECT_DIR, input);
    EditorWindows["Assets"] = std::make_unique<AssetsExplorer>(gui, MARIANA_PROJECT_DIR, *thumbnails);
    auto inspector = std::make_unique<InspectorWindow>(gui, assets, scene, undo);
    inspectorWindow = inspector.get();
    inspector->SetSelectEntity([this](Entity entity) {
        DeselectEntity();
        SelectEntity(entity.RawId());
    });
    EditorWindows["Inspector"] = std::move(inspector);
    EditorWindows["Loaded Assets"] = std::make_unique<LoadedAssetsWindow>(gui, assets, [this](AssetID id) {
        inspectorWindow->InspectAsset(id);
        EditorWindows["Inspector"]->SetOpen(true);
    });
    EditorWindows["Render Pipeline"] = std::make_unique<RenderPipelineWindow>(gui, sceneView->Effects(), sceneRenderer, *cam, kDefaultPipeline);
    EditorWindows["Lighting"] = std::make_unique<LightingWindow>(gui, sceneRenderer.lighting);
    EditorWindows["Hierarchy"] = std::make_unique<HierarchyWindow>(
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

    Logger::Info("OnStart done");
}

void EditorApp::OnUpdate(float deltaTime)
{
    thumbnails->Update();

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

    // Game time: animations only move while the game plays. Transforms changed here are propagated
    // once, in OnRender.
    animationSystem.Run(gameClock.DeltaTime());

    sceneCameras.clear();
    cameraSystem.Run();
}

void EditorApp::OnGUI()
{
    gui.PreUpdateGUI();
    ImGuizmo::BeginFrame();

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

    DrawTopMenu();
    // The views in a fixed order, Scene last: the first time they dock, it ends up the open tab.
    for (auto& [name, window] : EditorWindows)
    {
        if (window.get() != sceneWindow && window.get() != gameWindow)
            window->Draw();
    }
    gameWindow->Draw();
    sceneWindow->Draw();
}

void EditorApp::DrawSceneOverlay(ViewportWindow& window)
{
    DrawCameraFrustums(window);

    bool gizmoHit = false;
    if (selectedEntityID != static_cast<uint64_t>(-1) && selectedEntity.Has<WorldXform>())
    {
        WorldXform& form = *selectedEntity.Get<WorldXform>();
        glm::mat4 world = glm::make_mat4(form.model);

        const bool manipulated = DrawGizmo(world, cam->View(), cam->Projection(), window);
        gizmoHit = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
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
    }

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
            if (ImGui::MenuItem("New", "Ctrl+N")) {/* TODO */}
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {/* TODO */}
            if (ImGui::MenuItem("Save", "Ctrl+S")) {/* TODO */}
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {/* TODO */}
            ImGui::Separator();
            if (ImGui::MenuItem("Preferences...", "Ctrl+,")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {/* set a flag to quit */}
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
            if (ImGui::MenuItem("Cube")) SpawnPrimitive(BuiltinMesh::Cube, "Cube");
            if (ImGui::MenuItem("Sphere")) SpawnPrimitive(BuiltinMesh::Sphere, "Sphere");
            if (ImGui::MenuItem("Plane")) SpawnPrimitive(BuiltinMesh::Plane, "Plane");
            ImGui::Separator();
            if (ImGui::MenuItem("Camera")) SpawnCamera();
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
            if (ImGui::MenuItem("Loaded Assets")) { EditorWindows["Loaded Assets"]->SetOpen(!EditorWindows["Loaded Assets"]->IsOpen()); }
            if (ImGui::MenuItem("Render Pipeline")) { EditorWindows["Render Pipeline"]->SetOpen(!EditorWindows["Render Pipeline"]->IsOpen()); }
            if (ImGui::MenuItem("Lighting")) { EditorWindows["Lighting"]->SetOpen(!EditorWindows["Lighting"]->IsOpen()); }
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

        ImGui::EndMainMenuBar();
    }
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
        gameClock.Stop();
    else
        gameClock.Play();
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
    scene.Update(0.0f);

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
    gui.KillGui();
}

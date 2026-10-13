#include <Editor/EditorApp.hpp>
#include <Mariana/Foundation/FileReader.hpp>
#include <Editor/EditorCameraController.hpp>
#include <Mariana/Importers/GLTFLoader.hpp>
#include <Mariana/Rendering/BuiltinAssets.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>
#include <Mariana/Rendering/RenderPipeline.hpp>
#include <Mariana/Rendering/Components.hpp>
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
effects(renderer.graph, sceneRenderer.CameraBuffer())
{
    AnimationPlayer::RegisterComponents();
    SceneRenderer::RegisterComponents();

    scene.UpdateComponentRegistry();

    cam = std::make_unique<EditorCameraController>(input);
    cam->SetViewportSize(m_Window.GetWidth(), m_Window.GetHeight());

    // The scene renderer adds the scene passes; effects add ambient occlusion and the post-process
    // chain that ends on the screen.
    renderer.graph.SetBackbufferFormat(m_Window.GetWindowFormat());
    renderer.graph.SetOutputSize(m_Window.GetWidth(), m_Window.GetHeight());
    sceneRenderer.Init(renderer.graph);
    GLTF::GLTFLoader::RegisterLoader(assets);

    // The effects, their settings, the exposure and the shadow settings come from a pipeline file.
    RenderPipelineDesc pipeline;
    std::string pipelineError;
    if (RenderPipelineDesc::Load(kDefaultPipeline, pipeline, pipelineError))
        pipeline.Apply(effects, sceneRenderer, cam.get());
    else
        Logger::Error("Could not load the render pipeline: " + pipelineError);

    animationSystem = scene.CreateSystem<AnimationPlayer>([](Entity e, AnimationPlayer& player, float dt) {
        player.Update(e, dt);
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

    m_Window.GetSurface();
    cam->SetViewportSize(w, h);
    // The graph recreates every screen-sized texture before the next frame.
    renderer.graph.SetOutputSize(w, h);
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

void EditorApp::OnStart()
{
    Logger::Info("Starting");
    gui.InitGui(m_Window);

    EditorWindows["Stats"] = std::make_unique<StatsWindow>(gui);
    EditorWindows["Assets"] = std::make_unique<AssetsExplorer>(gui, MARIANA_PROJECT_DIR);
    auto inspector = std::make_unique<InspectorWindow>(gui, assets);
    inspectorWindow = inspector.get();
    EditorWindows["Inspector"] = std::move(inspector);
    EditorWindows["Loaded Assets"] = std::make_unique<LoadedAssetsWindow>(gui, assets);
    EditorWindows["Render Pipeline"] = std::make_unique<RenderPipelineWindow>(gui, effects, sceneRenderer, *cam, kDefaultPipeline);
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

    if (auto* editorCam = dynamic_cast<EditorCameraController*>(cam.get())) {
        editorCam->SetPosition({-10.0f, 3.0f, -10.0f});
        editorCam->SetYawPitch(glm::half_pi<float>() / 2, -glm::half_pi<float>() / 5);
    } 

    Logger::Info("OnStart done");
}

void EditorApp::OnUpdate(float deltaTime)
{
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
    
    // Picking reads the id under the cursor back only when clicking; the answer arrives a frame
    // or two later. Clicks on UI or the gizmo don't pick.
    double x, y;
    input.GetMousePosition(x, y);
    Texture& pickTarget = renderer.graph.GetTexture(SceneRenderer::kEntityIds);
    if (input.IsMouseButtonPressed(MouseButton::Left) &&
        !ImGui::GetIO().WantCaptureMouse && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() &&
        x > 0 && y > 0 && x < m_Window.GetWidth() && y < m_Window.GetHeight())
    {
        pickTarget.RequestPixelReadback(static_cast<int>(x), static_cast<int>(y));
    }

    uint64_t pickedID;
    if (pickTarget.TryGetPixelReadback(pickedID))
    {
        // 0: nothing was drawn there (the prepass clears to 0, and flecs never uses id 0).
        DeselectEntity();
        if (pickedID != 0 && pickedID != UINT64_MAX)
            SelectEntity(pickedID);
    }

    // Transforms changed here are propagated once, in OnRender.
    animationSystem.Run(deltaTime);
}

void EditorApp::OnGUI()
{
    gui.PreUpdateGUI();
    if(selectedEntityID != static_cast<uint64_t>(-1) && selectedEntity.Has<WorldXform>())
    {
        WorldXform& form = *selectedEntity.Get<WorldXform>();
        glm::mat4 world = glm::make_mat4(form.model);

        if (DrawGizmo(world, cam->View(), cam->Projection()))
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

    DrawTopMenu();
    for (auto& [name, window] : EditorWindows)
    {
        window->Draw();
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
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {/* TODO */}
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {/* TODO */}
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
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
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

        ImGui::EndMainMenuBar();
    }
}

bool EditorApp::DrawGizmo(glm::mat4& transform, const glm::mat4& view, const glm::mat4& proj)
{
    ImGuizmo::BeginFrame();
    ImGuizmo::SetOrthographic(false);

    // 1. draw OUTSIDE any imgui window
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());

    // 2. cover the entire app window
    ImGuizmo::SetRect(
        0,
        0,
        ImGui::GetIO().DisplaySize.x,
        ImGui::GetIO().DisplaySize.y
    );

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
    sceneRenderer.Prepare(*cam);
    effects.Update();
    renderer.Render([this](void* pass) { gui.PostUpdateGUI(static_cast<wgpu::RenderPassEncoder*>(pass)); });
}

void EditorApp::OnShutdown()
{
    Logger::Info("Shutdown now");
    gui.KillGui();
}

#include <Editor/EditorApp.hpp>
#include <FileReader.hpp>
#include <Editor/EditorCameraController.hpp>
#include <AssetManager.hpp>
#include <Buffer.hpp>
#include <ECS.hpp>
#include <Prefab.hpp>
#include <AnimationPlayer.hpp>

#include "ImGuizmo.h"

#include <Editor/Windows/Stats.hpp>
#include <Editor/Windows/AssetsExplorer.hpp>
#include <Editor/Windows/HierarchyWindow.hpp>
#include <Editor/Windows/InspectorWindow.hpp>

#include <sstream>
#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>
#include <algorithm>
#include <array>
#include <cstdint>
#include <glm/gtc/quaternion.hpp>
namespace fs = std::filesystem;

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

#pragma region ShaderUniforms

UBO ubo{};
UniformBufferLayout uboLayout2(false, ubo, ubo.lightDir, ubo.lightVP);
Buffer uboBuffer(uboLayout2);

TransformData transformBuffer{};
UniformBufferLayout transformLayout2(true, transformBuffer, transformBuffer.modelMatrix, transformBuffer.normalMatrix, transformBuffer.entityID);
Buffer transformBufferBuffer(transformLayout2);

GLTFMaterialProperties materialsBuffer;
UniformBufferLayout materialsLayout2(true, materialsBuffer, materialsBuffer.baseColor, materialsBuffer.metallicFactor, materialsBuffer.roughnessFactor,
    materialsBuffer.normalMapStrength, materialsBuffer.occlusionStrength,
    materialsBuffer.emissiveFactor, materialsBuffer.alphaCutoff);
Buffer materialsBufferBuffer(materialsLayout2);

CameraInfo cameraInfo{};
UniformBufferLayout camLayout(false, cameraInfo, cameraInfo.proj, cameraInfo.view, cameraInfo.viewProj, cameraInfo.invView, cameraInfo.invProj, cameraInfo.invViewProj, cameraInfo.pos, cameraInfo.exposure);
Buffer cameraBuffer(camLayout);

BoneData boneData{};
StorageArrayLayout boneLayout(1024, boneData, boneData.model, boneData.normal);
Buffer boneBufferBuffer(boneLayout);


GLTF::Vertex v{};
VertexBufferLayout vertexLayout{v, v.position, v.normal, v.tangent, v.texcoord0, v.texcoord1, v.color0, v.boneIndices, v.boneWeights};

#pragma endregion

System WriteTransformBufferSystem;
System AnimationSystem;
Shader2 StandardPBRShader("StandardPBR");
Shader2 StandardSkyboxShader("StandardSkybox");
Shader2 ShadowMapShader("StandardShadowMap");

struct Skybox {};

static ImGuizmo::OPERATION CurrentGizmoOperation = ImGuizmo::TRANSLATE;

EditorApp::EditorApp(const std::string& name) : Application(name), 
renderpass(false, true, { TextureFormat::BGRA8Unorm, TextureFormat::RGBA16Uint }, m_Window.GetWidth(), m_Window.GetHeight()),
shadowpass(false, true, {  }, 8192 , 8192 ),
standardPBRPipeline()
{
    ECS::RegisterTag<ShadowCasterTag>("ShadowCasterTag");
    ECS::RegisterComponent<NameComponent>("NameComponent");
    ECS::RegisterComponent<LocalTRS>("LocalTRS");
    ECS::RegisterComponent<WorldXform>("WorldXform");
    ECS::RegisterComponent<TransformClock>("TransformClock");
    ECS::RegisterComponent<XformCache>("XformCache");
    ECS::RegisterComponent<MeshComponent>("MeshComponent");
    ECS::RegisterComponent<AnimationTargetEntity>("AnimationTargetEntity");
    ECS::RegisterComponent<AnimationPlayer>("AnimationPlayer");
    ECS::RegisterTag<Skybox>("Skybox");

    scene.UpdateComponentRegistry();

    cam = new EditorCameraController(input);
    uboBuffer.Build();
    transformBufferBuffer.Build();
    materialsBufferBuffer.Build();
    cameraBuffer.Build();
    boneBufferBuffer.Build();

    renderpass.Init();
    shadowpass.Init();

    ShadowMapShader.Group(0)
    .AddBuffer("UBO", 0, uboBuffer)
    .AddBuffer("ModelData", 1, transformBufferBuffer);

    ShadowMapShader.SetVertexStructLayout(vertexLayout);
    ShadowMapShader.SetWGSL(FileReader::LoadRawString("/Shaders/shadow.wgsl"));
    ShadowMapShader.SetRenderpass(&shadowpass);
    ShadowMapShader.Build(true);

    StandardPBRShader.Group(0)
    .AddBuffer("UBO", 0, uboBuffer)
    .AddBuffer("ModelData", 1, transformBufferBuffer)
    .AddBuffer("CameraInfo", 2, cameraBuffer)
    .AddBuffer("Material", 3, materialsBufferBuffer)
    .AddTexture("ShadowMap", 4, TextureType_Depth)
    .AddSampler("ShadowSampler", 5, true)
    .AddBuffer("BoneData", 6, boneBufferBuffer);

    StandardPBRShader.SetTexture("ShadowMap", shadowpass.GetDepthView());

    StandardPBRShader.Group(1)
    .AddTexture("albedoMap", 0, TextureType_2D)
    .AddTexture("normalMap", 1, TextureType_2D)
    .AddTexture("metallicRoughnessMap", 2, TextureType_2D)
    .AddTexture("occlusionMap", 3, TextureType_2D)
    .AddTexture("emissiveMap", 4, TextureType_2D)
    .AddSampler("sampler", 5);

    StandardPBRShader.SetMaterialGroup(1);
    StandardPBRShader.SetVertexStructLayout(vertexLayout);
    StandardPBRShader.SetWGSL(FileReader::LoadRawString("/Shaders/test.wgsl"));
    StandardPBRShader.SetRenderpass(&renderpass);

    StandardPBRShader.Build();

    boneBufferBuffer.Write(boneData, 0);

    Texture skyboxTex;
    skyboxTex.LoadCubeTexture({
        "/Textures/skybox/right.jpg",
        "/Textures/skybox/left.jpg",
        "/Textures/skybox/top.jpg",
        "/Textures/skybox/bottom.jpg",
        "/Textures/skybox/back.jpg",
        "/Textures/skybox/front.jpg",
    }, TextureFormat::RGBA8UnormSrgb);
    AssetManager::LoadedTextures.push_back(skyboxTex);

    StandardSkyboxShader.Group(0)
    .AddBuffer("CameraInfo", 0, cameraBuffer)
    .AddTexture("CubeMap", 1, TextureType_Cube)
    .AddSampler("Sampler", 2);

    StandardSkyboxShader.SetTexture("CubeMap", skyboxTex);

    StandardSkyboxShader.SetVertexStructLayout(vertexLayout);
    StandardSkyboxShader.SetWGSL(FileReader::LoadRawString("/Shaders/skybox.wgsl"));
    StandardSkyboxShader.SetRenderpass(&renderpass);
    StandardSkyboxShader.Build();

    GLTF::GLTFLoader::LoadGLTF(std::string(RESOURCE_DIR) + "/Models/SkyBox.glb", StandardPBRShader);
    scene.Instantiate((std::string("SkyBox")).c_str()).Add<MeshComponent>({0}).AddTag<Skybox>().SetScaleUniform(20).SetPosition(0, 2, 0);

    Prefab test = GLTF::GLTFLoader::LoadGLTF(std::string(RESOURCE_DIR) + "/Models/Rumba.glb", StandardPBRShader);
    Entity spawnedTest = scene.Instantiate(test);

    Prefab avocado = GLTF::GLTFLoader::LoadGLTF(std::string(RESOURCE_DIR) + "/Models/Avocado.glb", StandardPBRShader);
    scene.Instantiate(avocado);

    AnimationPlayer& testPlayer = *spawnedTest.Get<AnimationPlayer>();
    testPlayer.SetEntityRoot(spawnedTest);
    testPlayer.SetAnimation(&AssetManager::LoadedAnimations[0]);

    WriteTransformBufferSystem = scene.CreateSystem<WorldXform>([&](Entity ent, WorldXform& form, float dt){
        transformBuffer.modelMatrix = glm::make_mat4(form.model);
        transformBuffer.normalMatrix = glm::transpose(glm::inverse(glm::mat3(transformBuffer.modelMatrix)));
        transformBuffer.SetEntityID(ent.RawId());
        transformBufferBuffer.Write(transformBuffer, form.id);
    });

    AnimationSystem = scene.CreateSystem<AnimationPlayer>([&](Entity e, AnimationPlayer& player, float dt) {
        if (!player.m_CurrentAnimation)
            return;
        player.UpdateTime(dt);
        scene.ForEachDescendant(e, [&](Entity node){
            if (!node.Has<AnimationTargetEntity>())
                return;

            AnimationTargetEntity& target = *node.Get<AnimationTargetEntity>();
            player.UpdateEntity(node, target);
        });
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
    m_Window.GetSurface();
    renderpass.Recreate(w, h);
}

void EditorApp::OnStart()
{
    Logger::Info("Starting");

    EditorWindows["Stats"] = std::make_unique<StatsWindow>(gui);
    EditorWindows["Assets"] = std::make_unique<AssetsExplorer>(gui);
    auto inspector = std::make_unique<InspectorWindow>(gui);
    inspectorWindow = inspector.get();
    EditorWindows["Inspector"] = std::move(inspector);
    EditorWindows["Hierarchy"] = std::make_unique<HierarchyWindow>(
        gui,
        scene,
        selectedEntityID,
        [this](uint64_t id) {
            DeselectEntity();
            SelectEntity(id);
        },
        [this](const fs::path& glbPath) {
            try {
                Prefab prefab = GLTF::GLTFLoader::LoadGLTF(glbPath.string(), StandardPBRShader);
                Entity spawned = scene.Instantiate(prefab, glbPath.stem().string().c_str());
                scene.Update(0.0f);
                DeselectEntity();
                SelectEntity(spawned.RawId());
                Logger::Info("Spawned prefab from: " + glbPath.string());
            } catch (...) {
                Logger::Error("Failed to spawn prefab from dropped GLB: " + glbPath.string());
            }
        });

    if (auto* editorCam = dynamic_cast<EditorCameraController*>(cam)) {
        editorCam->SetPosition({-10.0f, 3.0f, -10.0f});
        editorCam->SetYawPitch(glm::half_pi<float>() / 2, -glm::half_pi<float>() / 5);
    } 

    renderer.PushRenderpass(&shadowpass);
    renderer.PushRenderpass(&renderpass);


    ubo.lightDir = glm::normalize(glm::vec3(1.0f, -1.0f, 0.0f));
    glm::vec3 sceneCenter = glm::vec3(0.0f);   
    float lightDistance = 50.0f;               

    glm::vec3 lightPos = sceneCenter - ubo.lightDir * lightDistance;

    glm::vec3 lightForward = glm::normalize(lightPos - sceneCenter);
    glm::vec3 up = (fabs(glm::dot(lightForward, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.99f)
        ? glm::vec3(0.0f, 0.0f, 1.0f)
        : glm::vec3(0.0f, 1.0f, 0.0f);

    ubo.lightVP =
    glm::orthoLH_ZO(-25.0f, 25.0f,
                    -25.0f, 25.0f,
                     0.1f, 200.0f) *
    glm::lookAtLH(lightPos, sceneCenter, up);

    
    uboBuffer.Write(ubo, 0);

    for (int i = 0; i < AssetManager::LoadedMaterialProperties.size(); i++)
        materialsBufferBuffer.Write(AssetManager::LoadedMaterialProperties[i], i);

    renderpass.renderSystem = scene.CreateSystem<MeshComponent, WorldXform>([&](Entity ent, MeshComponent& meshComp, WorldXform& form, float dt){
        IMesh* mesh = AssetManager::LoadedMeshes[meshComp.meshIndex].get();

        if (!ent.HasTag<Skybox>())
        {

            renderpass.SetShader2(StandardPBRShader);
            renderpass.SetMesh(mesh);
            renderpass.SetBufferIndex("ModelData", form.id);
            

            for (Submesh& sm : mesh->submeshes)
            {
              renderpass.SetMaterial2(StandardPBRShader, AssetManager::LoadedMaterials[sm.materialIndex]);
              renderpass.SetBufferIndex("Material", sm.materialIndex);
              renderpass.Draw(sm.indexCount, sm.startIndex);
            }
        }
        else
        {
            renderpass.SetShader2(StandardSkyboxShader);
            renderpass.SetMesh(mesh);

            for (Submesh& sm : mesh->submeshes)
            {
              renderpass.Draw(sm.indexCount, sm.startIndex);
            }
        }
    });

    shadowpass.renderSystem = scene.CreateSystem<MeshComponent, WorldXform, ShadowCasterTag>([&](Entity ent, MeshComponent& meshComp, WorldXform& form, ShadowCasterTag& tag, float dt){
        (void)tag;
        shadowpass.SetShader2(ShadowMapShader);
        IMesh* mesh = AssetManager::LoadedMeshes[meshComp.meshIndex].get();
        shadowpass.SetMesh(mesh);
        shadowpass.SetBufferIndex("ModelData", form.id);
        
        for (Submesh& sm : mesh->submeshes)
        {
          shadowpass.Draw(sm.indexCount, sm.startIndex);
        }
    });
    Logger::Info("OnStart done");
}



void EditorApp::OnUpdate(float deltaTime)
{
    static int materialsCount = 0;

    if (materialsCount != AssetManager::LoadedMaterialProperties.size())
    {
        materialsCount = AssetManager::LoadedMaterialProperties.size();
        for (int i = 0; i < AssetManager::LoadedMaterialProperties.size(); i++)
            materialsBufferBuffer.Write(AssetManager::LoadedMaterialProperties[i], i);
    }
    
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
        m_Window.GetSurface();
        renderpass.Recreate(m_Window.GetWidth(), m_Window.GetHeight());
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
    }
    
    double x, y;
    input.GetMousePosition(x, y);
    static uint64_t sampledPixel = UINT64_MAX;
    if(x > 0 && y > 0 && x < m_Window.GetWidth() && y < m_Window.GetHeight())
        sampledPixel = renderpass.GetRenderTarget(1).SamplePixel(x, y);

    if(input.IsMouseButtonPressed(MouseButton::Left) && sampledPixel != UINT64_MAX)
    {
        DeselectEntity();
        SelectEntity((uint64_t)sampledPixel);
        
    }
    else if (input.IsMouseButtonPressed(MouseButton::Left) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
    {
        DeselectEntity();
    }
    // StandardPBRShader.WriteToBuffer("CameraInfo", cam->GetCameraInfo(), 0);
    // StandardSkyboxShader.WriteToBuffer("CameraInfo", cam->GetCameraInfo(), 0);
    cameraInfo = cam->GetCameraInfo();
    cameraBuffer.Write(cameraInfo, 0);
    scene.Update(deltaTime);
    AnimationSystem.Run();
}

void EditorApp::OnGUI()
{
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

    if (selectedEntityID != static_cast<uint64_t>(-1) && selectedEntity.Has<WorldXform>())
    {
        transformBuffer.modelMatrix = glm::make_mat4(selectedEntity.Get<WorldXform>()->model);
        transformBuffer.normalMatrix = glm::transpose(glm::inverse(glm::mat3(transformBuffer.modelMatrix)));
        transformBuffer.SetEntityID(selectedEntity.RawId());
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
    float aspect = static_cast<float>(m_Window.GetWidth()) /
               static_cast<float>(m_Window.GetHeight());
    
    WriteTransformBufferSystem.Run();
    renderer.Render(&gui);
}

void EditorApp::OnShutdown()
{
    Logger::Info("Shutdown now");
}





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

#include <sstream>
#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include <array>
namespace fs = std::filesystem;

#pragma region Helpers

static bool DragOrInputFloat(const char* id, float* v, float speed, const char* fmt, float width)
{
    struct State { bool editing = false; };
    static std::unordered_map<ImGuiID, State> s;

    ImGuiID iid = ImGui::GetID(id);
    State& st = s[iid];
    bool changed = false;

    ImGui::SetNextItemWidth(width);

    if (!st.editing)
    {
        changed |= ImGui::DragFloat(id, v, speed, 0.0f, 0.0f, fmt);

        // Click without dragging -> switch to text mode
        if (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit())
        {
            st.editing = true;
            ImGui::SetKeyboardFocusHere(0);
        }
    }
    else
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), fmt, static_cast<double>(*v));

        bool submit = ImGui::InputText(id, buf, IM_ARRAYSIZE(buf),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        if (submit || ImGui::IsItemDeactivated())
        {
            *v = std::strtof(buf, nullptr);
            st.editing = false;
            changed = true;
        }
    }

    return changed;
}




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

EditorApp::EditorApp(const std::string& name) : Application(name), 
renderpass(false, true, { TextureFormat::BGRA8Unorm, TextureFormat::R32Uint }, m_Window.GetWidth(), m_Window.GetHeight()),
shadowpass(false, true, {  }, 8192 , 8192 ),
standardPBRPipeline()
{
    ECS::RegisterTag<ShadowCasterTag>("ShadowCasterTag");
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
    Prefab test = GLTF::GLTFLoader::LoadGLTF(std::string(RESOURCE_DIR) + "/Models/Rumba.glb", StandardPBRShader);
    scene.Instantiate((std::string("SkyBox")).c_str()).Add<MeshComponent>({0}).AddTag<Skybox>().SetScaleUniform(20).SetPosition(0, 2, 0);
    Entity spawnedTest = scene.Instantiate(test);

    AnimationPlayer& testPlayer = *spawnedTest.Get<AnimationPlayer>();
    testPlayer.SetEntityRoot(spawnedTest);
    testPlayer.SetAnimation(&AssetManager::LoadedAnimations[0]);

    WriteTransformBufferSystem = scene.CreateSystem<WorldXform>([&](Entity ent, WorldXform& form, float dt){
        transformBuffer.modelMatrix = glm::make_mat4(form.model);
        transformBuffer.normalMatrix = glm::transpose(glm::inverse(glm::mat3(transformBuffer.modelMatrix)));
        transformBuffer.entityID = ent.RawId();
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
    if(input.IsKeyPressed(Key::F11))
    {
        m_Window.ToggleFullscreen();
        m_Window.GetSurface();
        renderpass.Recreate(m_Window.GetWidth(), m_Window.GetHeight());
    }

    
    double x, y;
    input.GetMousePosition(x, y);
    static uint32_t sampledPixel = 0;
    if(x > 0 && y > 0 && x < m_Window.GetWidth() && y < m_Window.GetHeight())
        sampledPixel = renderpass.GetRenderTarget(1).SamplePixel(x, y);

    if(input.IsMouseButtonPressed(MouseButton::Left) && sampledPixel != 4294967295)
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
    if(selectedEntityID != -1)
    {
        glm::mat4 world = glm::make_mat4(selectedEntity.Get<WorldXform>()->model);

        DrawGizmo(world, cam->View(), cam->Projection());

        glm::mat4 local = world;
        Entity parent = scene.Parent(selectedEntity);
        if (parent.IsValid()) {
            glm::mat4 parentWorld = glm::make_mat4(parent.Get<WorldXform>()->model);
            local = glm::inverse(parentWorld) * world;
        }

        glm::vec3 translation, rotation, scale;

        float matrix[16];
        memcpy(matrix, glm::value_ptr(local), sizeof(matrix));
        ImGuizmo::DecomposeMatrixToComponents(matrix,
            glm::value_ptr(translation),
            glm::value_ptr(rotation),
            glm::value_ptr(scale));

        selectedEntity.SetPosition(translation.x, translation.y, translation.z);
        selectedEntity.SetRotationEuler(rotation.x, rotation.y, rotation.z);
        selectedEntity.SetScale(scale.x, scale.y, scale.z);
    }

    DrawTopMenu();
    for (auto& [name, window] : EditorWindows)
    {
        window->Draw();
    }

    ImGui::Begin("Hierachy"); 
    scene.ForEachRoot([&](Entity e){
        DrawEntityNode(e);
    });
    ImGui::End();

    ImGui::Begin("Inspector"); 
    if (selectedEntityID == -1)
    {
        ImGui::Text("Select an entity to show it here");
    }
    else
    {
        DrawInspector(selectedEntity);
        transformBuffer.modelMatrix = glm::make_mat4(selectedEntity.Get<WorldXform>()->model);
        transformBuffer.normalMatrix = glm::transpose(glm::inverse(glm::mat3(transformBuffer.modelMatrix)));
        transformBuffer.entityID = selectedEntity.RawId();
    }
    ImGui::End();
}

void EditorApp::SelectEntity(uint64_t id)
{
    if(ImGuizmo::IsOver() || ImGuizmo::IsUsing()) return;
    selectedEntityID = id;
    selectedEntity = scene.FromId(id);
}

void EditorApp::DeselectEntity()
{
    if(ImGuizmo::IsOver() || ImGuizmo::IsUsing()) return;
    selectedEntityID = -1;
}

#pragma region GUI


void EditorApp::DrawEntityNode(Entity& e)
{
    const char* name = e.GetName();
    if (!name || !*name) name = "<error_name>";

    ImGuiTreeNodeFlags flags = 
        ImGuiTreeNodeFlags_OpenOnArrow | 
        ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_SpanAvailWidth |
        (e.HasChildren() ? 0 : ImGuiTreeNodeFlags_Leaf) |
        (selectedEntityID == e.RawId() ? ImGuiTreeNodeFlags_Selected : 0);
    ImGui::PushID((ImGuiID)(uintptr_t)e.RawId());
    bool open = ImGui::TreeNodeEx("label", flags, "%s", name);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)  && !ImGui::IsItemToggledOpen()) {
        DeselectEntity();
        SelectEntity(e.RawId());
        
    }

    if (ImGui::BeginPopupContextItem("entity_ctx")) {
        if (ImGui::MenuItem("Select")) selectedEntityID = e.RawId();
        ImGui::EndPopup();
    }

    if (open)
    {
        scene.ForEachChild(e, [&](Entity c){
            DrawEntityNode(c);
        });
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void EditorApp::DrawInspector(Entity& e)
{
    ImGui::TextDisabled("Name");
    ImGui::SameLine(0, 16);
    ImGui::SetNextItemWidth(-1);

    char entityName[128] = {};
    strcpy(entityName, e.GetName());
    ImGui::InputText("##entity_name", entityName, 128);

    ImGui::Separator();
    
    const char* header = "⚙  Transform";
    if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen))
    {
        LocalTRS L_TRS = *(e.Get<LocalTRS>());
        float* pos   = L_TRS.pos;
        float* rotDeg = L_TRS.rot_euler;
        float* scl   = L_TRS.scl;

        if (ImGui::BeginTable("##transform_table", 2, ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_NoBordersInBody))
        {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("values", ImGuiTableColumnFlags_WidthStretch);

            auto DrawVec3Row = [](const char* label, float v[3], float resetX, float resetY, float resetZ, float speed = 0.1f)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(label);
                ImGui::TableSetColumnIndex(1);
            
                ImGui::PushID(label);
                float line_h = ImGui::GetFrameHeight();
                float btn_w  = line_h; // square reset buttons
                float full_w = ImGui::GetContentRegionAvail().x;
            
                // three equal fields (account for 3 buttons + inner spacing)
                float field_w = (full_w - btn_w*3.0f - ImGui::GetStyle().ItemInnerSpacing.x*6.0f) / 3.0f;
            
                // X
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 80, 80, 255));
                if (ImGui::Button("X", ImVec2(btn_w, line_h))) v[0] = resetX;
                ImGui::SameLine();
                DragOrInputFloat("##X", &v[0], speed, "%.3f", field_w);
                ImGui::PopStyleColor();
                ImGui::SameLine();
            
                // Y
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(110, 190, 110, 255));
                if (ImGui::Button("Y", ImVec2(btn_w, line_h))) v[1] = resetY;
                ImGui::SameLine();
                DragOrInputFloat("##Y", &v[1], speed, "%.3f", field_w);
                ImGui::PopStyleColor();
                ImGui::SameLine();
            
                // Z
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 140, 220, 255));
                if (ImGui::Button("Z", ImVec2(btn_w, line_h))) v[2] = resetZ;
                ImGui::SameLine();
                DragOrInputFloat("##Z", &v[2], speed, "%.3f", field_w);
                ImGui::PopStyleColor();
            
                ImGui::PopID();
            };


            DrawVec3Row("Position", pos,    0.0f, 0.0f, 0.0f, 0.1f);
            DrawVec3Row("Rotation", rotDeg, 0.0f, 0.0f, 0.0f, 0.5f);
            DrawVec3Row("Scale",    scl,    1.0f, 1.0f, 1.0f, 0.05f);

            e.SetPosition(pos[0], pos[1], pos[2]);
            e.SetRotationEuler(rotDeg[0], rotDeg[1], rotDeg[2]);
            e.SetScale(scl[0], scl[1], scl[2]);
            ImGui::EndTable();
        }
    }

    ImGui::Separator();
    ImGui::TextDisabled("Components");
    scene.ForEachComponent(e, [&](const ComponentView& c){
        const char* name = (c.name && *c.name) ? c.name : "<unnamed>";
        ImGui::BulletText("%s", name);
    });
    e.SetName(entityName);
}

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

    if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                             ImGuizmo::TRANSLATE, ImGuizmo::LOCAL, matrix))
    {
        transform = glm::make_mat4(matrix);
    }

    return ImGuizmo::IsUsing();
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


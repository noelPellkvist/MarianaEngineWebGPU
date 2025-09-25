#include <EditorApp.hpp>
#include <FileReader.hpp>
#include <moved_later/EditorCameraController.hpp>
#include <AssetManager.hpp>

#include <sstream>
#include <imgui.h>

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

#pragma endregion

EditorApp::EditorApp(const std::string& name) : Application(name), PBR_Shader(5), renderpass(true, true, m_Window.GetWindowFormat(), m_Window.GetWidth(), m_Window.GetHeight())
{
    cam = new EditorCameraController(input);
    auto avocado1 = scene.Instantiate("Fresh Avocado");
    
    auto avocado1C = scene.Instantiate("Fresh Avocado child of first").SetParent(avocado1);

    auto avocado1Cc1 = scene.Instantiate("Fresh Avocado child of first child 1").SetParent(avocado1C);
    auto avocado1Cc2 = scene.Instantiate("Fresh Avocado child of first child 2").SetParent(avocado1C);
    auto avocado1Cc3 = scene.Instantiate("Fresh Avocado child of first child 3 ").SetParent(avocado1C);
    auto avocado1Cc4 = scene.Instantiate("Fresh Avocado child of first child 4").SetParent(avocado1C);

    auto avocado2 = scene.Instantiate("Fresh Avocado (1)");
    auto avocado2c = scene.Instantiate("Fresh Avocado (1) child").SetParent(avocado2);

    scene.Update(0.f);
}

EditorApp::~EditorApp()
{

}

void EditorApp::OnStart()
{
    Logger::Info("Starting");

    if (auto* editorCam = dynamic_cast<EditorCameraController*>(cam)) {
        editorCam->SetPosition({0.0f, 0.0f, 0.0f});
        editorCam->SetYawPitch(glm::half_pi<float>(), 0.0f);
    }

    renderpass.Init();
    PBR_Shader.LoadShader(FileReader::LoadRawString("/Shaders/test.wgsl"), {m_Window.GetWindowFormat()});
    GLTF::GLTFLoader::LoadGLTF(std::string(RESOURCE_DIR) + "/Models/Avocado.glb", PBR_Shader);
    mesh = AssetManager::LoadedMeshes[0];
}

void EditorApp::OnUpdate(float deltaTime)
{
    if(input.IsKeyPressed(Key::F11))
    {
        m_Window.ToggleFullscreen();
        m_Window.GetSurface();
        renderpass.Recreate(m_Window.GetWidth(), m_Window.GetHeight());
    }
}

void EditorApp::OnGUI()
{
    DrawTopMenu();
    ImGui::Begin("Stats"); 
    float fps   = ImGui::GetIO().Framerate;
    float ms    = 1000.0f / fps;
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", ms, fps);
    ImGui::End();

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
    }
    ImGui::End();
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
        selectedEntityID = e.RawId();
        selectedEntity = e;
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
        static float pos[3]   = {0.0f, 0.0f, 0.0f};
        static float rotDeg[3]= {0.0f, 0.0f, 0.0f}; 
        static float scl[3]   = {1.0f, 1.0f, 1.0f};
    

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

                // three equal fields
                float field_w = (full_w - btn_w*3.0f - ImGui::GetStyle().ItemInnerSpacing.x*6.0f) / 3.0f;

                // X
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 80, 80, 255));
                if (ImGui::Button("X", ImVec2(btn_w, line_h))) v[0] = resetX;
                ImGui::SameLine();
                ImGui::SetNextItemWidth(field_w);
                ImGui::DragFloat("##X", &v[0], speed, 0, 0, "%.3f");
                ImGui::PopStyleColor();
                ImGui::SameLine();

                // Y
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(110, 190, 110, 255));
                if (ImGui::Button("Y", ImVec2(btn_w, line_h))) v[1] = resetY;
                ImGui::SameLine();
                ImGui::SetNextItemWidth(field_w);
                ImGui::DragFloat("##Y", &v[1], speed, 0, 0, "%.3f");
                ImGui::PopStyleColor();
                ImGui::SameLine();

                // Z
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 140, 220, 255));
                if (ImGui::Button("Z", ImVec2(btn_w, line_h))) v[2] = resetZ;
                ImGui::SameLine();
                ImGui::SetNextItemWidth(field_w);
                ImGui::DragFloat("##Z", &v[2], speed, 0, 0, "%.3f");
                ImGui::PopStyleColor();

                ImGui::PopID();
            };

            DrawVec3Row("Position", pos,    0.0f, 0.0f, 0.0f, 0.1f);
            DrawVec3Row("Rotation", rotDeg, 0.0f, 0.0f, 0.0f, 0.5f);
            DrawVec3Row("Scale",    scl,    1.0f, 1.0f, 1.0f, 0.05f);

            ImGui::EndTable();
        }
    }
    e.SetName(entityName);
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

#pragma endregion

void EditorApp::OnRender()
{
    float aspect = static_cast<float>(m_Window.GetWidth()) /
               static_cast<float>(m_Window.GetHeight());
    
    PBR_Shader.WriteToUBO(cam->View(), cam->Position(), aspect);
    
    renderer.Render(*cam, renderpass, gui, PBR_Shader);
}

void EditorApp::OnShutdown()
{
    Logger::Info("Shutdown now");
}
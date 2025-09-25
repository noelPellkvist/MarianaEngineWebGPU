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
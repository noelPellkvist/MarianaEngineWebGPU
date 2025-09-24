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
    LoadedTextures = GLTF::GLTFLoader::LoadTexturesFromFile(std::string(RESOURCE_DIR) + "/Models/Avocado.glb", PBR_Shader);
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
    ImGui::Begin("Hello, world!"); 
    float fps   = ImGui::GetIO().Framerate;
    float ms    = 1000.0f / fps;
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", ms, fps);
    ImGui::End();

    ImGui::Begin("Loaded images"); 
    float availWidth = ImGui::GetContentRegionAvail().x;

    for (size_t i = 0; i < LoadedTextures.size(); i++)
    {
        float texW = LoadedTextures[i].GetWidth();
        float texH = LoadedTextures[i].GetHeight();

        float aspect = texH / texW;
        float drawW = availWidth;
        float drawH = drawW * aspect;

        gui.DrawTexture(LoadedTextures[i], drawW, drawH);
    }
        
    ImGui::End();
}

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
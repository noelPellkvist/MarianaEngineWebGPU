#include <EditorApp.hpp>
#include <FileReader.hpp>
#include <sstream>

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

EditorApp::EditorApp(const std::string& name) : Application(name), PBR_Shader(5)
{
}

EditorApp::~EditorApp()
{

}

void EditorApp::OnStart()
{
    Logger::Info("Starting");
    renderpass.Init();
    PBR_Shader.LoadShader(FileReader::LoadRawString("/Shaders/test.wgsl"), {m_Window.GetWindowFormat()});
    material.InitMaterial(PBR_Shader, {"/Textures/Default_albedo.jpg", "/Textures/Default_normal.jpg", "/Textures/Default_AO.jpg", "/Textures/Default_metalRoughness.jpg", "/Textures/Default_emissive.jpg"});
    mesh = GLTF::GLTFLoader::LoadFromFile(std::string(RESOURCE_DIR) + "/Models/DamagedHelmet.glb");
    mesh.BuildMesh();
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

void EditorApp::OnRender()
{
    float aspect = static_cast<float>(m_Window.GetWidth()) /
               static_cast<float>(m_Window.GetHeight());
    
    
    
    PBR_Shader.WriteToUBO(cam.View(), cam.Position(), aspect);
    
    renderer.Render(renderpass, material, PBR_Shader, mesh.vertexBuffer, mesh.indexBuffer, mesh.IndexCount());
}

void EditorApp::OnShutdown()
{
    Logger::Info("Shutdown now");
}
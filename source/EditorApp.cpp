#include <EditorApp.hpp>
#include <FileReader.hpp>
#include <moved_later/EditorCameraController.hpp>

#include <sstream>

Texture albedo;
Texture normal;
Texture ambient;
Texture metalroughness;
Texture emmisive;

std::vector<Texture> gltfLoadedTextures;

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
    cam = new EditorCameraController(input);
}

EditorApp::~EditorApp()
{

}

void EditorApp::OnStart()
{
    Logger::Info("Starting");

    
    if (auto* editorCam = dynamic_cast<EditorCameraController*>(cam)) {
        glm::vec3 eye    = {0.0f, 0.0f, 0.0f};
        glm::vec3 target = {0.0f, 0.0f, 1.0f};

        editorCam->SetPosition(eye);
        editorCam->SetYawPitch(glm::half_pi<float>(), 0.0f);
    }

    renderpass.Init();
    PBR_Shader.LoadShader(FileReader::LoadRawString("/Shaders/test.wgsl"), {m_Window.GetWindowFormat()});

    albedo.LoadTexture("/Textures/Default_albedo.jpg", TextureFormat::RGBA8Unorm);
    normal.LoadTexture("/Textures/Default_normal.jpg", TextureFormat::RGBA8Unorm);
    ambient.LoadTexture("/Textures/Default_AO.jpg", TextureFormat::RGBA8Unorm);
    metalroughness.LoadTexture("/Textures/Default_metalRoughness.jpg", TextureFormat::RGBA8Unorm);
    emmisive.LoadTexture("/Textures/Default_emissive.jpg", TextureFormat::RGBA8Unorm);

    gltfLoadedTextures = GLTF::GLTFLoader::LoadTexturesFromFile(std::string(RESOURCE_DIR) + "/Models/DamagedHelmet.glb");

    material.InitMaterial(PBR_Shader, {albedo, normal, ambient, metalroughness, emmisive});
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
    
    PBR_Shader.WriteToUBO(cam->View(), cam->Position(), aspect);
    
    renderer.Render(renderpass, material, PBR_Shader, mesh.vertexBuffer, mesh.indexBuffer, mesh.IndexCount());
}

void EditorApp::OnShutdown()
{
    Logger::Info("Shutdown now");
}
// #include <iostream>

// #include <GLFW/glfw3.h>
// #if defined(__EMSCRIPTEN__)
// #include <emscripten/emscripten.h>
// #endif
// #include <dawn/webgpu_cpp_print.h>
// #include <webgpu/webgpu_cpp.h>

// #include <glm/glm.hpp>
// #include <glm/gtc/matrix_transform.hpp> 
// #include <glm/ext/matrix_clip_space.hpp>

// #include <chrono>

// #include <Init.hpp>
// #include <Window.hpp>
// #include <Shader.hpp>
// #include <Mesh.hpp>
// #include <FileReader.hpp>
// #include <UniformLayout.hpp>
// #include <Material.hpp>
// #include <Logger.hpp>

// #include <Renderpass.hpp>
// #include <Renderer.hpp>

// #include <moved_later/OBJLoader.hpp>
// #include <moved_later/GLTFLoader.hpp>
// #include <moved_later/IInput.hpp>
// #include <moved_later/EditorCameraController.hpp>
// #include <moved_later/GUI.hpp>

// #include <ECS.hpp>

// Window m_Window(1366, 768, "MARIANA MANNEN");
// Renderpass renderpass(true, true, windowFormat, m_Window.GetWidth(), m_Window.GetHeight());
// Shader PBR_Shader(5);
// Material material;
// Renderer renderer;

// IInput input(m_Window.GetWindow());
// EditorCameraController cam(input);
// GUI gui;

// Mesh<GLTF::Vertex, uint32_t> mesh16 = GLTF::GLTFLoader::LoadFromFile(std::string(RESOURCE_DIR) + "/Models/DamagedHelmet.glb");

// void InitGraphics() {
//   renderpass.Init();
//   PBR_Shader.LoadShader(FileReader::LoadRawString("/Shaders/test.wgsl"), {windowFormat});
//   material.InitMaterial(PBR_Shader, {"/Textures/Default_albedo.jpg", "/Textures/Default_normal.jpg", "/Textures/Default_AO.jpg", "/Textures/Default_metalRoughness.jpg", "/Textures/Default_emissive.jpg"});
//   mesh16.BuildMesh();
// }

// void Update()
// {

//   using clock = std::chrono::high_resolution_clock;

//   static auto lastTime = clock::now();
//   auto now = clock::now();
//   std::chrono::duration<float> elapsed = now - lastTime;
//   float dt = elapsed.count();
//   lastTime = now;
  
//   float aspect = static_cast<float>(m_Window.GetWidth()) /
//                static_cast<float>(m_Window.GetHeight());

//   cam.Update(dt);
//   PBR_Shader.WriteToUBO(cam.View(), cam.Position(), aspect);

//   if(input.IsKeyPressed(Key::F11))
//   {
//     m_Window.ToggleFullscreen();
//     m_Window.GetSurface();
//     renderpass.Recreate(m_Window.GetWidth(), m_Window.GetHeight());
//   }

//   renderer.Render(renderpass, material, PBR_Shader, mesh16.vertexBuffer, mesh16.indexBuffer, mesh16.IndexCount());

//   input.Update();
// }

// void Start() {
//   glm::vec3 eye    = {0.0f, 0.0f, 0.0f};
//   glm::vec3 target = {0.0f, 0.0f, 1.0f};

//   cam.SetPosition(eye);
//   cam.SetYawPitch(glm::half_pi<float>(), 0.0f);

//   m_Window.GetSurface();

//   InitGraphics();

//   gui.InitGui(m_Window);

// #if defined(__EMSCRIPTEN__)
//   emscripten_set_main_loop(Update, 0, false);
// #else
//   while (!m_Window.ShouldClose()) {

    
//     Update();
    
//     surface.Present();
//     instance.ProcessEvents();

    
//   }
// #endif
// }

#include <EditorApp.hpp>

int main() {

  // Scene scene;

  // auto root   = scene.Instantiate("Root");
  // auto parent = scene.Instantiate("Parent").SetParent(root)
  //                .SetPosition(0,2,0).SetRotationEuler(0,0.5f,0).SetScale(2,2,2);
  // auto a      = scene.Instantiate("A").SetParent(parent).SetPosition(1,0,0);
  // auto b      = scene.Instantiate("B").SetParent(parent).SetPosition(-1,0,0);

  // scene.ForEachChild(parent, [&](Entity c){
  //     printf("%s\n", c.GetName());
  // });

  // scene.Update(0.f);

  // Init();
  // Start();
  // gui.KillGui();
  EditorApp app("hejsan");
  app.Start();
}

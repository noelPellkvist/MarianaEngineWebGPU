#include "Application.hpp"
#include "Logging.hpp"

#include <webgpu/webgpu_cpp.h>

#include <iostream>
#include "Resources.hpp"
#include "Components/Transform.hpp"
#include "Loaders/GLTFLoader.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

#include <string>


Application::Application() : m_Window(1336, 768, "MARIANA"), scene("built_in_scene"), renderSystem(scene, m_Window.GetTargetFormat())
{
  gui = new GUI(m_Window.GetRawWindowPointer(), m_Window.GetTargetFormat(), renderSystem.GetShaders()[0].TransformData);
  LoadGLTFObject("rumba.glb", scene, renderSystem);
}

Application::~Application()
{
}

void Application::Render()
{
  wgpu::SurfaceTexture surfaceTexture;
  m_Window.GetCurrentTexture(&surfaceTexture);

  renderSystem.Draw(surfaceTexture);
}

void Application::Start()
{
  Logging::PrintSuccess("Start method called in application");
  #if defined(__EMSCRIPTEN__)
  auto callback = [](void *arg) {
    Application* pApp = reinterpret_cast<Application*>(arg);
    pApp->Render();
  };
  emscripten_set_main_loop_arg(callback, this, 0, true);
    #else
  while (!m_Window.ShouldClose())
  {
    m_Window.PollEvents();
    Update();
    m_Window.Present();
    instance.ProcessEvents();
  }
  #endif
}

void Application::Update()
{
  Render();
}
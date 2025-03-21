#include "Application.hpp"
#include "Logging.hpp"

#include <webgpu/webgpu_cpp.h>

#include <iostream>
#include "Resources.hpp"
#include "Components/Transform.hpp"

Application::Application() : m_Window(1336, 768, "MARIANA"), scene("built_in_scene"), renderSystem(scene, m_Window.GetTargetFormat())
{
  auto c = scene.CreateGameobject("Cube");
  renderSystem.RegisterComponent(c);
  scene.GetEntities().emplace<Mesh>(c, Resources::LoadObjMesh("monkey.obj", renderSystem.GetShaders()[0]));
}

Application::~Application()
{
}

void Application::Render()
{
  wgpu::SurfaceTexture surfaceTexture;
  m_Window.GetCurrentTexture(&surfaceTexture);

  //scene.DrawAllObjects(surfaceTexture);
  renderSystem.Draw(surfaceTexture);
}

void Application::Start()
{
  Logging::PrintSuccess("Start method called in application");
  while (!m_Window.ShouldClose())
  {
    m_Window.PollEvents();
    Update();
    m_Window.Present();
    instance.ProcessEvents();
  }
}

void Application::Update()
{
  Render();
}
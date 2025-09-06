#include <Window.hpp>
#include <webgpu/webgpu_glfw.h>
#include <Init.hpp>

Window::Window(uint32_t width, uint32_t height, std::string title) : 
    m_Width(width),
    m_Height(height),
    m_Title(title)
{
    if (!glfwInit()) {
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_Window = glfwCreateWindow(m_Width, m_Height, m_Title.c_str(), nullptr, nullptr);
}

Window::~Window()
{
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

void Window::GetSurface()
{
    surface = wgpu::glfw::CreateSurfaceForWindow(instance, m_Window);
    wgpu::SurfaceCapabilities capabilities;
    surface.GetCapabilities(adapter, &capabilities);
    windowFormat = capabilities.formats[0];

    wgpu::SurfaceConfiguration config{.device = device,
                                    .format = windowFormat,
                                    .width = m_Width,
                                    .height = m_Height,
                                    .presentMode = wgpu::PresentMode::Fifo};
    surface.Configure(&config);
}

bool Window::ShouldClose()
{
    glfwPollEvents();
    return glfwWindowShouldClose(m_Window);
}
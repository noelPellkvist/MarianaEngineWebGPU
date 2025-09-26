#include <Window.hpp>
#include <webgpu/webgpu_glfw.h>
#include <Init.hpp>

Window* Window::s_global = nullptr;

void Window::SetGlobal(Window* w) { s_global = w; }
Window* Window::GetGlobal()       { return s_global; }

Window::Window(uint32_t width, uint32_t height, std::string title) : 
    m_Width(width),
    m_Height(height),
    m_Title(title)
{
    if (!glfwInit()) {
        return;
    }
    Window::SetGlobal(this);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_Window = glfwCreateWindow(m_Width, m_Height, m_Title.c_str(), nullptr, nullptr);

    glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);
}

Window::~Window()
{
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

void Window::RegisterResizeCallback(std::function<void(int, int)> callback)
{
    m_ResizeCallback = std::move(callback);
}

void Window::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    // Recover our C++ instance
    Window* self = Window::GetGlobal();
    if (!self) return;

    // Update cached width/height
    self->m_Width = width;
    self->m_Height = height;

    // Call user-provided callback
    if (self->m_ResizeCallback) {
        self->m_ResizeCallback(width, height);
    }
}

void Window::ToggleFullscreen()
{
    m_IsFullscreen = !m_IsFullscreen;

    if (m_IsFullscreen)
    {
        // Save windowed position & size
        glfwGetWindowPos(m_Window, &m_WindowPosX, &m_WindowPosY);
        glfwGetWindowSize(m_Window, &m_WindowWidth, &m_WindowHeight);

        // Get primary monitor
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        // Go fullscreen
        glfwSetWindowMonitor(m_Window, monitor,
                             0, 0, 
                             mode->width, mode->height,
                             mode->refreshRate);
    }
    else
    {
        // Restore windowed mode
        glfwSetWindowMonitor(m_Window, nullptr,
                             m_WindowPosX, m_WindowPosY,
                             m_WindowWidth, m_WindowHeight,
                             0); // 0 = let GLFW pick refresh rate
    }
}

wgpu::TextureFormat Window::GetWindowFormat()
{
    return windowFormat;
}


void Window::GetSurface()
{
    if(!surface)
    {
        surface = wgpu::glfw::CreateSurfaceForWindow(instance, m_Window);
    }    
    else 
        surface.Unconfigure();
    

    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(m_Window, &fbWidth, &fbHeight);

    m_Width  = static_cast<uint32_t>(fbWidth);
    m_Height = static_cast<uint32_t>(fbHeight);
    
    wgpu::SurfaceCapabilities capabilities;
    surface.GetCapabilities(adapter, &capabilities);
    
    // Pick an sRGB format if available
    windowFormat = wgpu::TextureFormat::Undefined;
    for (uint32_t i = 0; i < capabilities.formatCount; ++i) {
        auto fmt = capabilities.formats[i];
        if (fmt == wgpu::TextureFormat::BGRA8UnormSrgb ||
            fmt == wgpu::TextureFormat::RGBA8UnormSrgb) {
            windowFormat = fmt;
            break;
        }
    }
    
    // Fallback: just take the first if no sRGB found
    if (windowFormat == wgpu::TextureFormat::Undefined) {
        windowFormat = capabilities.formats[0];
    }

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
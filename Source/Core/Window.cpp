#include "Window.hpp"
#include "GlobalVaribles.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#else
#include <webgpu/webgpu_glfw.h>
#endif
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "../External/stb_image.h"


Window::Window(int width, int height, std::string name) : 
    kWidth(width), 
    kHeight(height),
    m_Name(name)
{
    if (!glfwInit())
        return;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    m_Window = glfwCreateWindow(static_cast<uint32_t>(kWidth), static_cast<uint32_t>(kHeight), name.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(m_Window, this);


    GLFWimage images[1]; 
    images[0].pixels = stbi_load((std::string(RESOURCE_DIR) + "/Editor/Icon.png").c_str(), &images[0].width, &images[0].height, 0, 4);
    glfwSetWindowIcon(m_Window, 1, images); 
    stbi_image_free(images[0].pixels);

    //TODO: fix 

    // glfwSetFramebufferSizeCallback(m_Window, [this](GLFWwindow* window, int, int){
    //     // auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    //     // if (that != nullptr) that->WindowResized();
    //     WindowResized();

    // });

    #if defined(__EMSCRIPTEN__)
      wgpu::SurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
      canvasDesc.selector = "#canvas";

      wgpu::SurfaceDescriptor surfaceDesc{.nextInChain = &canvasDesc};
      surface = instance.CreateSurface(&surfaceDesc);
    #else
      m_Surface = wgpu::glfw::CreateSurfaceForWindow(instance, m_Window);
    #endif

    ConfigureSurface();
}

// void Window::WindowResized()
// {
//     std::cout << "Resizing" << std::endl;
// }

bool Window::ShouldClose()
{
    return glfwWindowShouldClose(m_Window);
}

void Window::PollEvents()
{
    glfwPollEvents();
}

void Window::Present()
{
    m_Surface.Present();
}

void Window::ConfigureSurface()
{
    glfwGetFramebufferSize(m_Window, &kWidth, &kHeight);
    wgpu::SurfaceCapabilities capabilities;
    m_Surface.GetCapabilities(adapter, &capabilities);
    m_SurfaceFormat = capabilities.formats[0];
    

    wgpu::SurfaceConfiguration config{
        .device = device,
        .format = m_SurfaceFormat,
        .width = (uint32_t)kWidth,
        .height = (uint32_t)kHeight
      };
    m_Surface.Configure(&config);
}

void Window::GetCurrentTexture(wgpu::SurfaceTexture* surfaceTexture)
{
    m_Surface.GetCurrentTexture(surfaceTexture);
}

Window::~Window()
{
    glfwDestroyWindow(m_Window);
}
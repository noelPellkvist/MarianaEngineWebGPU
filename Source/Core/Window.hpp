#pragma once
#include <webgpu/webgpu_cpp.h>
#include <string>
#include <GLFW/glfw3.h>

class Window
{
    public:
        Window(int width, int height, std::string name);
        ~Window();

        void ConfigureSurface();
        void PollEvents();
        void Present();
        void GetCurrentTexture(wgpu::SurfaceTexture* surfaceTexture);
        //void WindowResized();

        bool ShouldClose();
        wgpu::TextureFormat& GetTargetFormat() { return m_SurfaceFormat; }

    private:
        int kWidth, kHeight;
        std::string m_Name;

        GLFWwindow* m_Window;
        wgpu::Surface m_Surface;
        wgpu::TextureFormat m_SurfaceFormat;

        
};
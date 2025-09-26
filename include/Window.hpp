#pragma once
#include <webgpu/webgpu_cpp.h>
#include <GLFW/glfw3.h>
#include <string>
#include <functional>

class Window {
    public:
        Window(uint32_t width, uint32_t height, std::string title);
        ~Window();

        void GetSurface();

        bool ShouldClose();
        void ToggleFullscreen();

        const uint32_t GetHeight() { return m_Height; }
        const uint32_t GetWidth() { return m_Width; }

        wgpu::TextureFormat GetWindowFormat();

        GLFWwindow* GetWindow() { return m_Window; };

        void RegisterResizeCallback(std::function<void(int, int)> callback);
        static void   SetGlobal(Window* w);
        static Window* GetGlobal();
    
    private:
        uint32_t m_Width;
        uint32_t m_Height;
        std::string m_Title;
        GLFWwindow* m_Window;

        bool m_IsFullscreen = false;

        int m_WindowPosX = 0;
        int m_WindowPosY = 0;
        int m_WindowWidth = 0;
        int m_WindowHeight = 0;

        std::function<void(int, int)> m_ResizeCallback;

        static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
        
        static Window* s_global;
};
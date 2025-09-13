#pragma once
#include <webgpu/webgpu_cpp.h>
#include <GLFW/glfw3.h>
#include <string>


class Window {
    public:
        Window(uint32_t width, uint32_t height, std::string title);
        ~Window();

        void GetSurface();

        bool ShouldClose();

        const uint32_t GetHeight() { return m_Height; }
        const uint32_t GetWidth() { return m_Width; }

        GLFWwindow* GetWindow() { return m_Window; };
    
    private:
        uint32_t m_Width;
        uint32_t m_Height;
        std::string m_Title;
        GLFWwindow* m_Window;
};
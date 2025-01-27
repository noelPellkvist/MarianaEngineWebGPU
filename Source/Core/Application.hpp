#pragma once

#include "GlobalVaribles.hpp"
#include "Window.hpp"
#include "Renderer/Layer.hpp"

#include <glm.hpp>
#include <vector>
#include <GLFW/glfw3.h>



class Application
{
    public:
        Application();
        ~Application();     

        void Start();
        void Update();

    private:
        Window m_Window;
        RenderLayer m_RenderLayer;

        void Render();
};
#pragma once
#include <string>
#include <Logger.hpp>
#include <Window.hpp>
#include <moved_later/IInput.hpp>
#include <Renderpass.hpp>
#include <moved_later/EditorCameraController.hpp>

class Application
{
    public:
        Application(const std::string& name = "Game");
        virtual ~Application();

        void Start();

    protected:
        virtual void OnStart() {}
        virtual void OnUpdate(float deltaTime) {}
        virtual void OnRender() {}
        virtual void OnShutdown() {}

        void Quit();
        
        Window m_Window;
        IInput input;
        Renderpass renderpass;
        EditorCameraController cam;

    private:
        bool m_Running;
        std::string m_Name;
        

        void Initalize();
        void MainLoop();
        void Shutdown();
};
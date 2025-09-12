#pragma once
#include <string>
#include <Logger.hpp>
#include <Window.hpp>

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

    private:
        bool m_Running;
        std::string m_Name;
        Window m_Window;

        void Init();
        void MainLoop();
        void Shutdown();
};
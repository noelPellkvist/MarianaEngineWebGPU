#pragma once
#include <string>
#include <Logger.hpp>
#include <Window.hpp>
#include <Renderpass.hpp>
#include <ICamera.hpp>

#include <GUI.hpp>
#include <IInput.hpp>
#include <AssetsSystem/AssetManager.hpp>

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
        virtual void OnGUI() {}
        virtual void OnShutdown() {}

        void Quit();
        
        Window m_Window;
        IInput input;
        ICamera* cam;
        GUI gui;
        // Declared in the base class so it outlives everything a derived app owns (scenes,
        // components) that may still hold asset handles.
        AssetManager2 assets;

    private:
        bool m_Running;
        std::string m_Name;
        

        void Initalize();
        void MainLoop();
        void Shutdown();
};
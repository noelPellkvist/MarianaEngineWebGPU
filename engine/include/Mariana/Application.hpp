#pragma once
#include <memory>
#include <Mariana/Foundation/GameClock.hpp>
#include <Mariana/Foundation/Logger.hpp>
#include <Mariana/Platform/Window.hpp>
#include <Mariana/Graphics/Renderpass.hpp>
#include <Mariana/Rendering/ICamera.hpp>

#include <Mariana/Platform/IInput.hpp>
#include <Mariana/Assets/AssetManager.hpp>

// The base of an app (the editor, a game): window, input, assets, the game clock and the main loop,
// which ticks the clock, calls OnUpdate, updates `cam`, then OnGUI and OnRender every frame. The app
// chooses its window. OnUpdate gets the real frame time; game systems step with gameClock.DeltaTime().
class Application
{
    public:
        explicit Application(const WindowSettings& window);
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
        // Set by the app; updated by the main loop after OnUpdate.
        std::unique_ptr<ICamera> cam;
        // Declared in the base class so it outlives everything a derived app owns (scenes,
        // components) that may still hold asset handles.
        AssetManager2 assets;
        // Runs only while the game plays (the app calls Play; the editor from its toolbar).
        GameClock gameClock;

    private:
        bool m_Running;
        

        void Initalize();
        void MainLoop();
        void Shutdown();
};
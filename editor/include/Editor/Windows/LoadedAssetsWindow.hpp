#pragma once

#include <Editor/Windows/EditorWindow.hpp>
#include <Mariana/Assets/AssetManager.hpp>

// Lists every asset the AssetManager knows about, with its state and handle count.
class LoadedAssetsWindow final : public EditorWindow
{
public:
    LoadedAssetsWindow(GUI& gui, AssetManager2& assets);
    void Draw() override;

private:
    AssetManager2& m_assets;
    bool m_showUnloaded = false;
};

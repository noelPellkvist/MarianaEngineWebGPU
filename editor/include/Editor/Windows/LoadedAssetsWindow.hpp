#pragma once

#include <Editor/Windows/EditorWindow.hpp>
#include <Mariana/Assets/AssetManager.hpp>

#include <functional>

// Lists every asset the AssetManager knows about, with its state and handle count. Clicking a row
// calls `openAsset` (the editor shows it in the inspector).
class LoadedAssetsWindow final : public EditorWindow
{
public:
    LoadedAssetsWindow(GUI& gui, AssetManager2& assets, std::function<void(AssetID)> openAsset);
    void Draw() override;

private:
    AssetManager2& m_assets;
    std::function<void(AssetID)> m_openAsset;
    bool m_showUnloaded = false;
};

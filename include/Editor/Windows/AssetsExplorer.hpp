#pragma once

#include "Editor/Windows/EditorWindow.hpp"
#include <Texture.hpp>

#include <string>
#include <unordered_map>

class AssetsExplorer final : public EditorWindow
{
public:
    AssetsExplorer(GUI& gui);
    void Draw() override;

private:
    std::unordered_map<std::string, Texture> AssetsTextures;

    void DrawAssetsWindow();
    void LoadFileTextures();
    void LoadFileTexture(const std::string& path);
};

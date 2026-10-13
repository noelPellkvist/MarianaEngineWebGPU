#pragma once

#include "Editor/Windows/EditorWindow.hpp"
#include <StandardPBR.hpp>

// Live controls for the sun and the sky light.
class LightingWindow final : public EditorWindow
{
public:
    LightingWindow(GUI& gui, LightingSettings& lighting);
    void Draw() override;

private:
    LightingSettings& m_lighting;
};

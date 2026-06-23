#pragma once

#include "Editor/Windows/EditorWindow.hpp"

class StatsWindow final : public EditorWindow
{
public:
    StatsWindow(GUI& gui);
    void Draw() override;
};

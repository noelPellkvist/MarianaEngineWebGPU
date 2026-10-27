#pragma once

#include <Editor/Windows/EditorWindow.hpp>

#include <functional>

class StatsWindow final : public EditorWindow
{
public:
    StatsWindow(GUI& gui, std::function<void()> openProfiler);
    void Draw() override;

private:
    std::function<void()> m_OpenProfiler;
};

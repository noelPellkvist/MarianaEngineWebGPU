#pragma once

#include "Editor/Windows/EditorWindow.hpp"
#include <PostProcess.hpp>

// Live controls for exposure and bloom.
class PostProcessWindow final : public EditorWindow
{
public:
    PostProcessWindow(GUI& gui, PostProcessPipeline& postProcess);
    void Draw() override;

private:
    PostProcessPipeline& m_postProcess;
};

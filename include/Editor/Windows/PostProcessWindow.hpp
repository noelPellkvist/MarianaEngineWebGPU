#pragma once

#include "Editor/Windows/EditorWindow.hpp"
#include <PostProcess.hpp>
#include <AmbientOcclusion.hpp>

// Live controls for exposure, bloom and ambient occlusion.
class PostProcessWindow final : public EditorWindow
{
public:
    PostProcessWindow(GUI& gui, PostProcessPipeline& postProcess, AmbientOcclusion& ambientOcclusion);
    void Draw() override;

private:
    PostProcessPipeline& m_postProcess;
    AmbientOcclusion& m_ambientOcclusion;
};

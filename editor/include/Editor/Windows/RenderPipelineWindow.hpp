#pragma once

#include <Editor/Windows/EditorWindow.hpp>
#include <Mariana/Rendering/Effect.hpp>
#include <Mariana/Rendering/SceneRenderer.hpp>

#include <string>

class ICamera;

// The render pipeline: .pipeline files (resources/Pipelines) to load, save and save as, the camera
// exposure, the scene renderer's settings, then every effect with the controls its file declares;
// effects can be added from resources/Effects, toggled, reordered and removed.
class RenderPipelineWindow final : public EditorWindow
{
public:
    // `pipelinePath` is the file the running pipeline came from ("Save" writes it).
    RenderPipelineWindow(GUI& gui, EffectStack& effects, SceneRenderer& sceneRenderer, ICamera& camera, std::string pipelinePath);
    void Draw() override;

private:
    EffectStack& m_effects;
    SceneRenderer& m_sceneRenderer;
    ICamera& m_camera;
    std::string m_pipelinePath;
    char m_saveName[64] = {};
    std::string m_status;

    void DrawPipelineFile();
    void DrawSceneSettings();
    void DrawEffect(EffectInstance& effect, bool& removed);
    void LoadPipeline(const std::string& path);
    void SavePipeline(const std::string& path);
};

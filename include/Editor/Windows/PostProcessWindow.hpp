#pragma once

#include "Editor/Windows/EditorWindow.hpp"
#include <Rendering/Effect.hpp>

class ICamera;

// The effect stack: camera exposure, then every effect with the controls its file declares;
// effects can be added from resources/Effects, toggled, reordered and removed.
class PostProcessWindow final : public EditorWindow
{
public:
    PostProcessWindow(GUI& gui, EffectStack& effects, ICamera& camera);
    void Draw() override;

private:
    EffectStack& m_effects;
    ICamera& m_camera;

    void DrawEffect(EffectInstance& effect, bool& removed);
};

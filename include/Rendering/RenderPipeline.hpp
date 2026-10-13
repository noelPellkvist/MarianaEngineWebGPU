#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include <Rendering/Effect.hpp>
#include <Rendering/SceneRenderer.hpp>

class ICamera;

// A render pipeline as data: the scene renderer's settings, the camera exposure and the effect
// chain with its parameter values. Load one from a .pipeline file, build or edit one in code (a
// script can do the same), apply it to a running renderer, or capture what is running and save it.
//
// File format (JSON):
//   {
//     "name": "Default",
//     "exposure": 1.5,
//     "scene": { "shadows": true, "shadowDistance": 80, "shadowMapSize": 2048 },
//     "effects": [
//       { "effect": "/Effects/Bloom.effect", "enabled": true, "parameters": { "intensity": 1.0 } }, ...
//     ]
//   }
// Everything is optional: missing settings keep their defaults, missing parameters the effect
// file's defaults. Parameter values are numbers, booleans or arrays (vectors and colors).
struct RenderPipelineDesc
{
    struct Parameter
    {
        std::string name;
        EffectParamType type = EffectParamType::Float; // how it is written to a file
        glm::vec4 value{0.0f};
    };
    struct Effect
    {
        std::string path; // effect file, like "/Effects/Bloom.effect"
        bool enabled = true;
        std::vector<Parameter> parameters;
    };

    std::string name;
    float exposure = 1.0f;
    SceneRendererSettings scene;
    std::vector<Effect> effects;

    // Paths are relative to the resources folder, like "/Pipelines/Default.pipeline".
    static bool Load(const std::string& path, RenderPipelineDesc& out, std::string& error);
    bool Save(const std::string& path, std::string& error) const;

    // Makes the effect stack, the scene renderer and the camera's exposure (when given) match this.
    void Apply(EffectStack& effects, SceneRenderer& scene, ICamera* camera = nullptr) const;
    // What is running now, e.g. to save it.
    static RenderPipelineDesc Capture(const EffectStack& effects, const SceneRenderer& scene, const ICamera* camera = nullptr);
};

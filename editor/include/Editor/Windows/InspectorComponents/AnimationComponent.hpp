#pragma once

#include <Mariana/Scene/ECS.hpp>
#include <Mariana/Animation/AnimationGraph.hpp>

#include <functional>

class AssetManager2;

namespace InspectorComponents
{
    // What the Animator section's graph buttons do: open a graph in the graph editor, or make a
    // new graph for the entity's Animator (and assign it).
    struct AnimatorActions
    {
        std::function<void(const AssetHandle<AnimationGraph>&)> editGraph;
        std::function<void(Entity)> newGraph;
    };
}

namespace InspectorComponents
{
    void DrawAnimationComponent(Entity& entity, AssetManager2& assets, const AnimatorActions& actions);
}
#pragma once

#include <Mariana/Scene/ECS.hpp>
#include <Mariana/Animation/AnimationGraph.hpp>

#include <functional>
#include <vector>

class AssetManager2;
class UndoStack;

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
    // The Animator section; its clips' events are edited as undo steps.
    void DrawAnimationComponent(Entity& entity, AssetManager2& assets, UndoStack& undo, const AnimatorActions& actions);

    // A clip's events: a strip as long as the clip with a mark per event and the time now, then a row
    // per event (name, time, remove) and "Add at <now>". True when it changed them.
    bool DrawClipEvents(std::vector<AnimationEvent>& events, float duration, float now);
}
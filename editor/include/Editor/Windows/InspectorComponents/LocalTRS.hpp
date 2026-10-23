#pragma once

#include <Mariana/Scene/ECS.hpp>

class UndoStack;

namespace InspectorComponents
{
    // Position, rotation (Euler degrees) and scale; each drag or typed value is one undo step.
    void DrawLocalTRS(Entity& entity, UndoStack& undo);
}
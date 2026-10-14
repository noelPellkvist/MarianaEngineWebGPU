#pragma once

#include <Mariana/Scene/ECS.hpp>

class AssetManager2;

namespace InspectorComponents
{
    // The Mesh Renderer section: the mesh, whether it casts shadows, and the material in each slot
    // (picked from the loaded materials). The materials themselves are edited below the components.
    void DrawMeshComponent(Entity& entity, AssetManager2& assets);
}
#pragma once

#include <Mariana/Scene/ECS.hpp>

class AssetManager2;

namespace InspectorComponents
{
    // The mesh and a material editor per submesh; images dropped on a texture slot load through `assets`.
    void DrawMeshComponent(Entity& entity, AssetManager2& assets);
}
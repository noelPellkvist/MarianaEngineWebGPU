#pragma once

#include "../Scene.hpp"
#include "../Components/RenderSystem.hpp"

#include <string>

void LoadGLTFObject(std::string name, Scene& scene, RenderSystem& rendersystem);
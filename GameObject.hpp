#pragma once
#include <webgpu/webgpu_cpp.h>
#include <vector>
#include <string>
#include "Mesh.hpp"

class GameObject
{
    private:
    std::vector<GameObject> children;
    std::string name;
    wgpu::BindGroup* bindGroup;
    Mesh mesh;

    public:
    GameObject();
    GameObject(std::string name, std::string meshName, wgpu::BindGroup* group);
    ~GameObject();
    void Draw(wgpu::RenderPassEncoder& renderPass);
};
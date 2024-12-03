#pragma once
#include <webgpu/webgpu_cpp.h>
#include <vector>
#include <string>
#include "Mesh.hpp"
#include <glm.hpp>

class GameObject
{
    private:
    std::vector<GameObject> children;
    
    
    Mesh mesh;

    public:
    std::string name;
    wgpu::BindGroup* bindGroup;
    GameObject();
    GameObject(std::string name, std::string meshName, wgpu::BindGroup* group);
    GameObject(std::string name, Mesh& mesh);
    ~GameObject();
    void Draw(wgpu::RenderPassEncoder& renderPass);
    glm::vec3 position, rotation, scale;
    glm::mat4 modelMatrix;
};
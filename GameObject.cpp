#include "GameObject.hpp"
#include "Resources.h"
#include <iostream>
#include <gtc/matrix_transform.hpp>

GameObject::GameObject() : name("Empty"), bindGroup(nullptr)
{
    //Resources::LoadOBJMesh(std::string("/") + name);
}

GameObject::GameObject(std::string name, std::string meshName, wgpu::BindGroup* group) : name(name), bindGroup(group)
{
    mesh = Resources::LoadOBJMesh(std::string("/") + meshName);
    mesh.BuildMesh();
    position = glm::vec3(0);
    rotation = glm::vec3(0);
    scale = glm::vec3(1);
}

GameObject::~GameObject()   
{

}

void GameObject::Draw(wgpu::RenderPassEncoder& renderPass)
{
    // glm::vec3 position, rotation, scale;
    // glm::mat4 modelMatrix;
    modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, position);
    glm::vec3 rotationRadians = glm::radians(rotation);
    modelMatrix = glm::rotate(modelMatrix, rotationRadians.z, glm::vec3(0, 0, 1));  // Rotate around Z-axis
    modelMatrix = glm::rotate(modelMatrix, rotationRadians.x, glm::vec3(1, 0, 0));  // Rotate around X-axis
    modelMatrix = glm::rotate(modelMatrix, rotationRadians.y, glm::vec3(0, 1, 0));  // Rotate around Y-axis
    modelMatrix = glm::scale(modelMatrix, scale);

    renderPass.SetVertexBuffer(0, mesh.GetVertexBuffer(), 0, mesh.GetVertexBuffer().GetSize());
    renderPass.SetIndexBuffer(mesh.GetIndexBuffer(), wgpu::IndexFormat::Uint16, 0, mesh.GetIndexBuffer().GetSize());
    renderPass.SetBindGroup(0, *bindGroup, 0, nullptr);
    renderPass.DrawIndexed(mesh.getIndexCount(), 1, 0, 0);

    for(GameObject& g : children) g.Draw(renderPass);
}

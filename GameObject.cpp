#include "GameObject.hpp"
#include "Resources.h"
#include <iostream>

GameObject::GameObject() : name("Empty"), bindGroup(nullptr)
{
    //Resources::LoadOBJMesh(std::string("/") + name);
}

GameObject::GameObject(std::string name, std::string meshName, wgpu::BindGroup* group) : name(name), bindGroup(group)
{
    mesh = Resources::LoadOBJMesh(std::string("/") + meshName);
    mesh.BuildMesh();
}

GameObject::~GameObject()   
{

}

void GameObject::Draw(wgpu::RenderPassEncoder& renderPass)
{
    renderPass.SetVertexBuffer(0, mesh.GetVertexBuffer(), 0, mesh.GetVertexBuffer().GetSize());
    renderPass.SetIndexBuffer(mesh.GetIndexBuffer(), wgpu::IndexFormat::Uint16, 0, mesh.GetIndexBuffer().GetSize());
    renderPass.SetBindGroup(0, *bindGroup, 0, nullptr);
    renderPass.DrawIndexed(mesh.getIndexCount(), 1, 0, 0);

    for(GameObject& g : children) g.Draw(renderPass);
}

#pragma once
#include "../Buffers/UniformBuffer.hpp"
#include <webgpu/webgpu_cpp.h>
#include <GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>
#include <entt/entt.hpp>
#include "../Components/Transform.hpp"
#include "../Components/Relationship.hpp"

class GUI
{
    public:
        GUI(GLFWwindow* window, wgpu::TextureFormat format, UniformBuffer& TransfomBuffer);
        ~GUI();

        void DrawGUI(wgpu::RenderPassEncoder renderPass, entt::registry& reg);
        void DrawHierachry(Transform& transform, const Relationship& relationship, entt::registry& reg);
        void DrawGizmo();

    private:
        Transform* selectedTransform = nullptr;
        UniformBuffer& m_TransfomBuffer;
};
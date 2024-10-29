#pragma once
#include <GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>
#include <iostream>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include <webgpu/webgpu_glfw.h>
#endif
namespace MarianaEngine
{
	namespace Core
	{
		struct ApplicationInfo
		{
			uint32_t Width = 512;
			uint32_t Height = 512;
			wgpu::Instance* instance;
			wgpu::Adapter* adapter;
			wgpu::Device* device;
			const char* name;
		};
		class Application 
		{ 
		public:
			void Init(ApplicationInfo info);
			void MainLoop();
			void CleanUp();
		private:
			uint32_t kWidth = 512;
			uint32_t kHeight = 512;
			wgpu::Instance instance;
			wgpu::Adapter adapter;
			wgpu::Device device;
			wgpu::Queue queue;
			wgpu::Surface surface;
			wgpu::TextureFormat format;
			wgpu::RenderPipeline pipeline;
			GLFWwindow* window;

			wgpu::Buffer vertexBuffer;
			wgpu::Buffer indexBuffer;
			uint32_t indexCount;

			bool isRunning();

			void Render();
			void InitGraphics();
			void ConfigureSurface();
			void CreateRenderPipeline();
			void InitializeVertexBuffer();
		};
	}
}

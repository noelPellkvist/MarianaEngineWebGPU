#pragma once
#include <GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>
#include <iostream>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include <webgpu/webgpu_glfw.h>
#endif
#include <webgpu/webgpu_glfw.h>
namespace MarianaEngine
{
	namespace Core
	{
		using namespace wgpu;
		struct ApplicationInfo
		{
			const char* name;
		};

		class Application
		{
			
		public:
			Application(const ApplicationInfo& info);
			~Application();

			
		private:
			ApplicationInfo info;
			Device m_device;
			Queue m_queue;
			GLFWwindow* m_window;
			Surface surface;

			void Init();
			void MainLoop();
			void CleanUp();
			bool isRunning();
			void SetupHardware();

			void SetupWindow();

			WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const* options);
			WGPUDevice requestDeviceSync(WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor);
		};
	}
}
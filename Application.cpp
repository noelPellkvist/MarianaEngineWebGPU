#include "Application.h"
#include "Resource.h"

#include <string>

namespace MarianaEngine
{
	namespace Core
	{
		Application::Application(const ApplicationInfo& info) : info(info)
		{
			SetupWindow();
			SetupHardware();
		}

		Application::~Application()
		{
			CleanUp();
		}

		void Application::Init()
		{
			
			m_queue = m_device.GetQueue();		
#ifdef WEBGPU_BACKEND_EMSCRIPTEN
			auto callback = [](void* arg) {
				//                   ^^^ 2. We get the address of the app in the callback.
				Application* pApp = reinterpret_cast<Application*>(arg);
				//                  ^^^^^^^^^^^^^^^^ 3. We force this address to be interpreted
				//                                      as a pointer to an Application object.
				pApp->MainLoop(); // 4. We can use the application object
			};
			emscripten_set_main_loop_arg(callback, &app, 0, true);
#else
			while (isRunning()) {
				MainLoop();
			}
#endif
		}

		void Application::MainLoop()
		{
			glfwPollEvents();
		}

		void Application::CleanUp()
		{
			glfwDestroyWindow(m_window);
		}

		bool Application::isRunning()
		{
			return !glfwWindowShouldClose(m_window);
		}

		void Application::SetupWindow()
		{
			glfwInit();
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
			m_window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);
			
		}

#pragma region Adapter/Device
		void Application::SetupHardware()
		{
			WGPUInstanceDescriptor desc = {};
			desc.nextInChain = nullptr;

#ifdef WEBGPU_BACKEND_EMSCRIPTEN
			WGPUInstance instance = wgpuCreateInstance(nullptr);
#else 
			WGPUInstance instance = wgpuCreateInstance(&desc);
#endif

			if (!instance) {
				std::cerr << "Could not initialize WebGPU!" << std::endl;
				return;
			}
			surface = glfw::CreateSurfaceForWindow(instance, m_window);
			std::cout << "WGPU instance: " << instance << std::endl;

			std::cout << "Requesting adapter..." << std::endl;
			WGPURequestAdapterOptions adapterOpts = {};
			adapterOpts.nextInChain = nullptr;
			adapterOpts.compatibleSurface = surface.Get();
			WGPUAdapter adapter = requestAdapterSync(instance, &adapterOpts);
			std::cout << "Got adapter: " << adapter << std::endl;

			// We no longer need to use the instance once we have the adapter
			wgpuInstanceRelease(instance);

			std::cout << "Requesting device..." << std::endl;
			WGPUDeviceDescriptor deviceDesc = {};
			deviceDesc.nextInChain = nullptr;
			deviceDesc.label = WGPUStringView("My Device"); 
			deviceDesc.requiredFeatureCount = 0; 
			deviceDesc.requiredLimits = nullptr; 
			deviceDesc.defaultQueue.nextInChain = nullptr;
			deviceDesc.defaultQueue.label = WGPUStringView("Default queue");

			deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, WGPUStringView message, void* pUserData) {
				std::cout << "Device lost: reason " << reason;
				if (message.length > 0) std::cout << " (" << message.data << ")";
				std::cout << std::endl;
			};
			WGPUDevice device = requestDeviceSync(adapter, &deviceDesc);
			std::cout << "Got device: " << device << std::endl;

			// A function that is invoked whenever there is an error in the use of the device
			auto onDeviceError = [](WGPUErrorType type, WGPUStringView message, void* /* pUserData */) {
				std::cout << "Uncaptured device error: type " << type;
				if (message.length > 0) std::cout << " (" << message.data << ")";
				std::cout << std::endl;
			};
			wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError, nullptr /* pUserData */);
			
			wgpuAdapterRelease(adapter);
			m_device = (Device)device;
			Init();
		}

		WGPUAdapter Application::requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const* options) {
			struct UserData {
				WGPUAdapter adapter = nullptr;
				bool requestEnded = false;
			};
			UserData userData;
#ifdef __EMSCRIPTEN__
			auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message, void* pUserData) {
				UserData& userData = *reinterpret_cast<UserData*>(pUserData);
				if (status == WGPURequestAdapterStatus_Success) {
					userData.adapter = adapter;
				}
				else {
					std::cout << "Could not get WebGPU adapter: " << message << std::endl;
				}
				userData.requestEnded = true;
			};
#else
			auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* pUserData) {
				UserData& userData = *reinterpret_cast<UserData*>(pUserData);
				if (status == WGPURequestAdapterStatus_Success) {
					userData.adapter = adapter;
				}
				else {
					std::cout << "Could not get WebGPU adapter: " << message.data << std::endl;
				}
				userData.requestEnded = true;
			};
#endif



			wgpuInstanceRequestAdapter(
				instance,
				options,
				onAdapterRequestEnded,
				(void*)&userData
			);

#ifdef __EMSCRIPTEN__
			while (!userData.requestEnded) {
				emscripten_sleep(100);
			}
#endif

			assert(userData.requestEnded);

			return userData.adapter;
		}

		WGPUDevice Application::requestDeviceSync(WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor) {
			struct UserData {
				WGPUDevice device = nullptr;
				bool requestEnded = false;
			};
			UserData userData;
#ifdef __EMSCRIPTEN__
			auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, char const* message, void* pUserData) {
				UserData& userData = *reinterpret_cast<UserData*>(pUserData);
				if (status == WGPURequestDeviceStatus_Success) {
					userData.device = device;
				}
				else {
					std::cout << "Could not get WebGPU device: " << message << std::endl;
				}
				userData.requestEnded = true;
			};
#else
			auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* pUserData) {
				UserData& userData = *reinterpret_cast<UserData*>(pUserData);
				if (status == WGPURequestDeviceStatus_Success) {
					userData.device = device;
				}
				else {
					std::cout << "Could not get WebGPU device: " << message.data << std::endl;
				}
				userData.requestEnded = true;
			};
#endif


			wgpuAdapterRequestDevice(
				adapter,
				descriptor,
				onDeviceRequestEnded,
				(void*)&userData
			);

#ifdef __EMSCRIPTEN__
			while (!userData.requestEnded) {
				emscripten_sleep(100);
			}
#endif

			assert(userData.requestEnded);

			return userData.device;
		}
#pragma endregion
	}
}
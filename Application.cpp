#include "Application.h"

namespace MarianaEngine
{
	namespace Core
	{
		const char shaderCode[] = R"(
    @vertex fn vs_main(@location(0) in_vertex_position: vec2f) -> @builtin(position) vec4f {
    return vec4f(in_vertex_position, 0.0, 1.0);
	}
    @fragment fn fs_main() -> @location(0) vec4f {
        return vec4f(1, 0, 0, 1);
    }
)";

		void Application::Init(ApplicationInfo info)
		{
			std::cout << "Initializing application!" << std::endl;
			kWidth = info.Width;
			kHeight = info.Height;
			instance = *info.instance;
			adapter = *info.adapter;
			device = *info.device;
			queue = device.GetQueue();

			if (!glfwInit()) {
				return;
			}

			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			GLFWwindow* window =
				glfwCreateWindow(kWidth, kHeight, info.name, nullptr, nullptr);

#if defined(__EMSCRIPTEN__)
			wgpu::SurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
			canvasDesc.selector = "#canvas";

			wgpu::SurfaceDescriptor surfaceDesc{ .nextInChain = &canvasDesc };
			surface = instance.CreateSurface(&surfaceDesc);
#else
			surface = wgpu::glfw::CreateSurfaceForWindow(instance, window);
#endif

			InitGraphics();
			InitializeVertexBuffer();

#if defined(__EMSCRIPTEN__)
			emscripten_set_main_loop(MainLoop, 0, false);
#else
			while (!glfwWindowShouldClose(window)) {
				glfwPollEvents();
				MainLoop();
				surface.Present();
				instance.ProcessEvents();
			}
#endif
			CleanUp();
		}

		void Application::InitGraphics() {
			ConfigureSurface();
			CreateRenderPipeline();
		}

		void Application::ConfigureSurface() {
			wgpu::SurfaceCapabilities capabilities;
			surface.GetCapabilities(adapter, &capabilities);
			format = capabilities.formats[0];

			wgpu::SurfaceConfiguration config{
				.device = device,
				.format = format,
				.width = kWidth,
				.height = kHeight };
			surface.Configure(&config);
		}

		void Application::CreateRenderPipeline() {
			wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
			wgslDesc.code = shaderCode;

			wgpu::ShaderModuleDescriptor shaderModuleDescriptor{
				.nextInChain = &wgslDesc };
			wgpu::ShaderModule shaderModule =
				device.CreateShaderModule(&shaderModuleDescriptor);

			wgpu::VertexBufferLayout vertexBufferLayout;
			wgpu::VertexAttribute positionAttrib;

			positionAttrib.shaderLocation = 0;
			positionAttrib.format = wgpu::VertexFormat::Float32x2;
			positionAttrib.offset = 0;

			vertexBufferLayout.attributeCount = 1;
			vertexBufferLayout.attributes = &positionAttrib;

			vertexBufferLayout.arrayStride = 2 * sizeof(float);
			vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;

			

			

			

			wgpu::RenderPipelineDescriptor descriptor{};

			descriptor.vertex.bufferCount = 1;
			descriptor.vertex.buffers = &vertexBufferLayout;

			descriptor.vertex.module = shaderModule;
			descriptor.vertex.entryPoint = "vs_main";
			descriptor.vertex.constantCount = 0;
			descriptor.vertex.constants = nullptr;

			descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
			descriptor.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
			descriptor.primitive.frontFace = wgpu::FrontFace::CCW;

			descriptor.primitive.cullMode = wgpu::CullMode::None; //Culling here

			wgpu::FragmentState fragmentState{};
			fragmentState.module = shaderModule;
			fragmentState.entryPoint = "fs_main";
			fragmentState.constantCount = 0;
			fragmentState.constants = nullptr;

			wgpu::BlendState blendState;
			blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
			blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
			blendState.color.operation = wgpu::BlendOperation::Add;
			blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
			blendState.alpha.dstFactor = wgpu::BlendFactor::One;
			blendState.alpha.operation = wgpu::BlendOperation::Add;

			wgpu::ColorTargetState colorTargetState{};
			colorTargetState.format = format;
			colorTargetState.blend = &blendState;
			colorTargetState.writeMask = wgpu::ColorWriteMask::All;

			fragmentState.targetCount = 1;
			fragmentState.targets = &colorTargetState;
			descriptor.fragment = &fragmentState;
			descriptor.depthStencil = nullptr;
			descriptor.multisample.count = 1;
			descriptor.multisample.alphaToCoverageEnabled = false;
			descriptor.layout = nullptr;

			pipeline = device.CreateRenderPipeline(&descriptor);
		}

		void Application::MainLoop()
		{
			Render();
		}

		void Application::Render() {
			wgpu::SurfaceTexture surfaceTexture;
			surface.GetCurrentTexture(&surfaceTexture);

			wgpu::RenderPassColorAttachment attachment{
				.view = surfaceTexture.texture.CreateView(),
				.loadOp = wgpu::LoadOp::Clear,
				.storeOp = wgpu::StoreOp::Store,
				.clearValue = { 0.05, 0.05, 0.05, 1.0 } };

			wgpu::RenderPassDescriptor renderpass{ .colorAttachmentCount = 1,
												  .colorAttachments = &attachment };

			wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
			wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);
			pass.SetPipeline(pipeline);
			pass.SetVertexBuffer(0, vertexBuffer, 0, vertexBuffer.GetSize());
			pass.Draw(vertexCount, 1, 0, 0);
			pass.End();
			wgpu::CommandBuffer commands = encoder.Finish();
			queue.Submit(1, &commands);
		}



		bool Application::isRunning()
		{ 
			return !glfwWindowShouldClose(window);
		}

		void Application::CleanUp()
		{
			vertexBuffer.Destroy();
		}

		void Application::InitializeVertexBuffer()
		{
			std::vector<float> vertexData = {
				// Define a first triangle:
				-0.5, -0.5,
				+0.5, -0.5,
				+0.0, +0.5,

				// Add a second triangle:
				-0.55f, -0.5,
				-0.05f, +0.5,
				-0.55f, +0.5
			};
			vertexCount = static_cast<uint32_t>(vertexData.size() / 2);

			// Create vertex buffer
			wgpu::BufferDescriptor bufferDesc;
			bufferDesc.size = vertexData.size() * sizeof(float);
			bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex; // Vertex usage here!
			bufferDesc.mappedAtCreation = false;
			vertexBuffer = device.CreateBuffer(&bufferDesc);

			// Upload geometry data to the buffer
			queue.WriteBuffer(vertexBuffer, 0, vertexData.data(), bufferDesc.size);
		}
	}
}
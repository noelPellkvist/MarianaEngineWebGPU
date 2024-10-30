#include "Application.h"
#include "Resource.h"

#include <string>

namespace MarianaEngine
{
	namespace Core
	{
		const char shaderCode[] = R"(
	struct MyUniforms {
    color: vec4f,
    time: f32,
	};
	@group(0) @binding(0) var<uniform> uMyUniforms: MyUniforms;
	struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
	};
	struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) normal: vec3f,
};
    @vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let ratio = 640.0 / 480.0;

    // We now move the scene depending on the time!
    var offset = vec2f(0, 0);
    offset += 0.3 * vec2f(cos(uMyUniforms.time), sin(uMyUniforms.time));

    out.position = vec4f(in.position.x + offset.x, (in.position.y + offset.y) * ratio, 0.0, 1.0);
    out.normal = in.normal;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	let color = in.normal * uMyUniforms.color.rgb;
    let corrected_color = pow(color, vec3f(2.2));
    return vec4f(corrected_color, uMyUniforms.color.a);
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

			
			InitializeVertexBuffer();
			InitGraphics();

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
			//wgpu::VertexAttribute positionAttrib;
			std::vector<wgpu::VertexAttribute> vertexAttribs(2);

			vertexAttribs[0].shaderLocation = 0;
			vertexAttribs[0].format = wgpu::VertexFormat::Float32x3;
			vertexAttribs[0].offset = 0;

			vertexAttribs[1].shaderLocation = 1; // @location(1)
			vertexAttribs[1].format = wgpu::VertexFormat::Float32x3; // different type!
			vertexAttribs[1].offset = 3 * sizeof(float); // non null offset!

			vertexBufferLayout.attributeCount = vertexAttribs.size();
			vertexBufferLayout.attributes = vertexAttribs.data();

			vertexBufferLayout.arrayStride = 6 * sizeof(float);
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

			wgpu::BindGroupLayoutEntry bindingLayout = {};
			bindingLayout.binding = 0;
			bindingLayout.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;

			bindingLayout.buffer.type = wgpu::BufferBindingType::Uniform;
			//bindingLayout.buffer.minBindingSize = 4 * sizeof(float);
			bindingLayout.buffer.minBindingSize = sizeof(MyUniforms);

			


			wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc{};
			bindGroupLayoutDesc.entryCount = 1;
			bindGroupLayoutDesc.entries = &bindingLayout;
			bindGroupLayout = device.CreateBindGroupLayout(&bindGroupLayoutDesc);

			wgpu::BindGroupEntry binding{};
			wgpu::BindGroupDescriptor bindGroupDesc{};
			bindGroupDesc.layout = bindGroupLayout;

			binding.binding = 0;
			binding.buffer = uniformBuffer;
			binding.offset = 0;
			binding.size = sizeof(MyUniforms);

			bindGroupDesc.entryCount = 1;
			bindGroupDesc.entries = &binding;
			bindGroup = device.CreateBindGroup(&bindGroupDesc);

			// Create the pipeline layout
			wgpu::PipelineLayoutDescriptor layoutDesc{};
			layoutDesc.bindGroupLayoutCount = 1;
			layoutDesc.bindGroupLayouts = &bindGroupLayout;
			layout = device.CreatePipelineLayout(&layoutDesc);
			descriptor.layout = layout;

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

			float t = static_cast<float>(glfwGetTime()); // glfwGetTime returns a double
			uniforms.time = t;
			queue.WriteBuffer(uniformBuffer, 0, &uniforms, sizeof(MyUniforms));

			wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
			wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);
			pass.SetPipeline(pipeline);
			pass.SetVertexBuffer(0, vertexBuffer, 0, vertexBuffer.GetSize());
			pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint16, 0, indexBuffer.GetSize());
			pass.SetBindGroup(0, bindGroup, 0, nullptr);
			pass.DrawIndexed(indexCount, 1, 0, 0);
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
			indexBuffer.Destroy();
			uniformBuffer.Destroy();
		}

		void Application::InitializeVertexBuffer()
		{
			std::vector<float> vertexData = {
				// x,   y,     r,   g,   b
			-0.5f, -0.5f, -0.3f, 1.0f, 1.0f, 1.0f,
			+0.5f, -0.5f, -0.3f, 1.0f, 1.0f, 1.0f,
			+0.5f, +0.5f, -0.3f, 1.0f, 1.0f, 1.0f,
			-0.5f, +0.5f, -0.3f, 1.0f, 1.0f, 1.0f,
			0.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.5f
			};
			std::vector<uint16_t> indexData = {
				0, 1, 2, 
				0, 2, 3,
				0, 1, 4,
				1, 2, 4,
				2, 3, 4,
				3, 0, 4
			};
			indexCount = static_cast<uint32_t>(indexData.size());

			// Create vertex buffer
			wgpu::BufferDescriptor bufferDesc;
			bufferDesc.size = vertexData.size() * sizeof(float);
			bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex; // Vertex usage here!
			bufferDesc.mappedAtCreation = false;
			vertexBuffer = device.CreateBuffer(&bufferDesc);

			// Upload geometry data to the buffer
			queue.WriteBuffer(vertexBuffer, 0, vertexData.data(), bufferDesc.size);

			bufferDesc.size = indexData.size() * sizeof(uint16_t);
			bufferDesc.size = (bufferDesc.size + 3) & ~3;
			bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index;
			bufferDesc.mappedAtCreation = false;
			indexBuffer = device.CreateBuffer(&bufferDesc);
			queue.WriteBuffer(indexBuffer, 0, indexData.data(), bufferDesc.size);

			bufferDesc.size = sizeof(MyUniforms);

			bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;

			bufferDesc.mappedAtCreation = false;
			uniformBuffer = device.CreateBuffer(&bufferDesc);
			
			uniforms.time = 1.0f;
			uniforms.color[0] = 0.0f;
			uniforms.color[1] = 1.0f;
			uniforms.color[2] = 0.4f;
			uniforms.color[3] = 1.0f;
			queue.WriteBuffer(uniformBuffer, 0, &uniforms, sizeof(MyUniforms));
		}
	}
}
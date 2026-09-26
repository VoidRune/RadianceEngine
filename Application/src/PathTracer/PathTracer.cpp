#include "PathTracer.h"
#include "RadianceEngine/Window/Input.h"
#include "RadianceEngine/Graphics/ShaderCompiler.h"
#include "RadianceEngine/Core/Log.h"
#include "RadianceEngine/Core/Timer.h"
#include "stb/stb_image_write.h"
#include <thread>

using namespace glm;

PathTracer::PathTracer(Rdn::Window* window, Rdn::Device* device, Rdn::PresentQueue* presentQueue, Rdn::ResourceAllocator* resourceAllocator, Rdn::RenderGraph* renderGraph)
{
	m_Window = window;
	m_Device = device;
	m_PresentQueue = presentQueue;
	m_ResourceAllocator = resourceAllocator;
	m_RenderGraph = renderGraph;

	m_Camera = std::make_unique<CameraFP>(m_Window);
	m_Camera->Position = glm::vec3(0.0f, 0.55f, -2.1f);
	m_Camera->Fov = 50.0f;
	m_Camera->Pitch = -5.0f;
	m_Camera->MovementSpeed = 1.0f;

	CreateSamplers();
	if (!CreatePipelines())
	{
		RDN_LOG_ERROR("Shader compilation failed, rendering is paused until the shaders compile (they reload on save, or press F5)");
	}
	CreateScene();
	CreateImages();

	m_GlobalDataBuffer = std::make_unique<Rdn::GpuRingBuffer>();
	m_ResourceAllocator->CreateGpuRingBuffer(m_GlobalDataBuffer.get(), Rdn::GpuRingBufferDesc{
		.ElementSize = sizeof(GlobalFrameData),
		.ElementCount = m_Device->GetFramesInFlightCount(),
		.UsageFlags = Rdn::BufferUsage::UniformBuffer,
		.MemoryProperty = Rdn::MemoryProperty::HostVisible,
		.Mapped = true
	});
}

void PathTracer::CreateScene()
{
	m_Scene = std::make_unique<Scene>(m_Device, m_ResourceAllocator);
	Scene& scene = *m_Scene;

	const ModelId plane = scene.LoadModel("res/3DModels/plane.obj");
	const ModelId cube = scene.LoadModel("res/3DModels/cube.obj");
	const ModelId sphere = scene.LoadModel("res/3DModels/sphere.obj");
	const ModelId bunny = scene.LoadModel("res/3DModels/bunny.obj");
	const ModelId dragon = scene.LoadModel("res/3DModels/dragon.obj");

	const TextureId granite = scene.LoadTexture("res/Images/Granite.jpg");
	std::vector<uint32_t> checker(768 * 1024);
	for (uint32_t y = 0; y < 1024; y++)
		for (uint32_t x = 0; x < 768; x++)
			checker[y * 768 + x] = (x / 64 + y / 64) % 2 ? 0xFF404040 : 0xFFD0D0D0;
	const TextureId checkerboard = scene.AddTexture(768, 1024, checker);

	const MediumId jade = scene.AddMedium({ .Absorption = { 4.0f, 0.4f, 3.0f }, .Scattering = glm::vec3(40.0f), .Anisotropy = 0.3f });
	const MediumId blueTint = scene.AddMedium({ .Absorption = { 3.0f, 1.2f, 0.3f } });
	const MediumId smoke = scene.AddMedium({ .Absorption = glm::vec3(0.2f), .Scattering = glm::vec3(5.0f), .Anisotropy = 0.4f });

	const MaterialId floor = scene.AddMaterial({ .Roughness = 0.45f, .Clearcoat = 0.6f, .ClearcoatRoughness = 0.06f, .Texture = checkerboard });
	const MaterialId whiteWall = scene.AddMaterial({ .BaseColor = glm::vec3(0.8f), .Roughness = 0.9f });
	const MaterialId redWall = scene.AddMaterial({ .BaseColor = { 1.0f, 0.2f, 0.3f }, .Roughness = 0.8f, .Texture = granite });
	const MaterialId blueWall = scene.AddMaterial({ .BaseColor = { 0.25f, 0.55f, 1.0f }, .Roughness = 0.9f });
	const MaterialId mirror = scene.AddMaterial({ .BaseColor = glm::vec3(0.95f), .Metallic = 1.0f, .Roughness = 0.02f });
	const MaterialId light = scene.AddMaterial({ .BaseColor = glm::vec3(0.0f), .Emission = glm::vec3(14.0f) });
	const MaterialId marble = scene.AddMaterial({ .BaseColor = glm::vec3(0.9f), .Roughness = 0.25f, .Texture = granite });
	const MaterialId jadeGlass = scene.AddMaterial({ .Roughness = 0.25f, .Transmission = 1.0f, .IOR = 1.62f, .Medium = jade });
	const MaterialId tintedGlass = scene.AddMaterial({ .Roughness = 0.0f, .Transmission = 1.0f, .IOR = 1.5f, .Medium = blueTint });
	const MaterialId frostedGlass = scene.AddMaterial({ .Roughness = 0.3f, .Transmission = 1.0f, .IOR = 1.5f });
	const MaterialId chrome = scene.AddMaterial({ .BaseColor = glm::vec3(0.95f), .Metallic = 1.0f, .Roughness = 0.02f });
	const MaterialId gold = scene.AddMaterial({ .BaseColor = { 1.0f, 0.78f, 0.34f }, .Metallic = 1.0f, .Roughness = 0.3f });
	const MaterialId carPaint = scene.AddMaterial({ .BaseColor = { 0.7f, 0.03f, 0.03f }, .Roughness = 0.45f, .Clearcoat = 1.0f, .ClearcoatRoughness = 0.03f });
	const MaterialId velvet = scene.AddMaterial({ .BaseColor = { 0.25f, 0.04f, 0.1f }, .Roughness = 1.0f, .Sheen = glm::vec3(1.0f) });
	const MaterialId blue = scene.AddMaterial({ .BaseColor = { 0.15f, 0.3f, 0.85f }, .Roughness = 0.2f });
	const MaterialId yellow = scene.AddMaterial({ .BaseColor = { 0.95f, 0.75f, 0.1f }, .Roughness = 0.6f });
	const MaterialId green = scene.AddMaterial({ .BaseColor = { 0.2f, 0.7f, 0.25f }, .Roughness = 0.35f });
	const MaterialId cyanGlow = scene.AddMaterial({ .BaseColor = glm::vec3(0.0f), .Emission = { 0.6f, 4.2f, 5.4f } });
	const MaterialId orangeGlow = scene.AddMaterial({ .BaseColor = glm::vec3(0.0f), .Emission = { 24.0f, 8.0f, 1.2f } });
	const MaterialId smokeVolume = scene.AddMaterial({ .Medium = smoke, .NullSurface = true });

	scene.AddInstance(plane, { .Position = { 0.0f, 0.0f, -0.8f }, .Scale = { 2.4f, 1.0f, 3.2f } }, floor); // FLOOR
	scene.AddInstance(plane, { .Position = { 0.0f, 1.2f, 0.0f }, .Rotation = { 180.0f, 0.0f, 0.0f }, .Scale = { 2.4f, 1.0f, 1.6f } }, whiteWall); // CEILING
	scene.AddInstance(plane, { .Position = { 0.0f, 0.6f, 0.8f }, .Rotation = { -90.0f, 0.0f, 0.0f }, .Scale = { 2.4f, 1.0f, 1.2f } }, blueWall); // BACK
	scene.AddInstance(plane, { .Position = { -1.2f, 0.6f, 0.0f }, .Rotation = { 0.0f, 0.0f, -90.0f }, .Scale = { 1.2f, 1.0f, 1.6f } }, redWall); // LEFT
	scene.AddInstance(plane, { .Position = { 1.2f, 0.6f, 0.0f }, .Rotation = { 0.0f, 0.0f, 90.0f }, .Scale = { 1.2f, 1.0f, 1.6f } }, mirror); // RIGHT
	scene.AddInstance(plane, { .Position = { 0.0f, 1.199f, 0.15f }, .Rotation = { 180.0f, 0.0f, 0.0f }, .Scale = { 0.7f, 1.0f, 0.45f } }, light); // LIGHT

	scene.AddInstance(cube, { .Position = { 0.0f, 0.05f, 0.45f }, .Scale = { 0.95f, 0.1f, 0.5f } }, marble); // PEDESTAL
	scene.AddInstance(dragon, { .Position = { 0.0f, 0.312f, 0.45f }, .Rotation = { 0.0f, 70.0f, 0.0f }, .Scale = glm::vec3(0.75f) }, jadeGlass);

	scene.AddInstance(cube, { .Position = { -0.72f, 0.32f, 0.38f }, .Rotation = { 0.0f, 20.0f, 0.0f }, .Scale = { 0.32f, 0.64f, 0.32f } }, frostedGlass); // TALL BOX
	scene.AddInstance(sphere, { .Position = { -0.72f, 0.77f, 0.38f }, .Scale = glm::vec3(0.26f) }, chrome);
	scene.AddInstance(cube, { .Position = { 0.75f, 0.17f, 0.3f }, .Rotation = { 0.0f, -18.0f, 0.0f }, .Scale = glm::vec3(0.34f) }, gold); // SHORT BOX
	scene.AddInstance(bunny, { .Position = { 0.77f, 0.261f, 0.3f }, .Rotation = { 0.0f, 200.0f, 0.0f }, .Scale = glm::vec3(2.4f) }, velvet);

	scene.AddInstance(sphere, { .Position = { -0.28f, 0.16f, -0.25f }, .Scale = glm::vec3(0.32f) }, tintedGlass);
	scene.AddInstance(sphere, { .Position = { 0.32f, 0.12f, -0.32f }, .Scale = glm::vec3(0.24f) }, carPaint);
	scene.AddInstance(sphere, { .Position = { 0.02f, 0.04f, -0.5f }, .Scale = glm::vec3(0.08f) }, cyanGlow);
	scene.AddInstance(cube, { .Position = { 0.62f, 0.162f, -0.14f }, .Scale = glm::vec3(0.32f) }, smokeVolume); // SMOKE
	scene.AddInstance(sphere, { .Position = { 0.62f, 0.12f, -0.14f }, .Scale = glm::vec3(0.08f) }, orangeGlow);
	scene.AddInstance(cube, { .Position = { -0.82f, 0.07f, -0.3f }, .Rotation = { 0.0f, 12.0f, 0.0f }, .Scale = glm::vec3(0.14f) }, blue); // TOY BLOCKS
	scene.AddInstance(cube, { .Position = { -0.8f, 0.21f, -0.31f }, .Rotation = { 0.0f, -20.0f, 0.0f }, .Scale = glm::vec3(0.14f) }, yellow);
	scene.AddInstance(cube, { .Position = { -0.83f, 0.35f, -0.29f }, .Rotation = { 0.0f, 35.0f, 0.0f }, .Scale = glm::vec3(0.14f) }, green);

	scene.Build();
	CreateSceneDescriptorSet();
}
void PathTracer::CreateSceneDescriptorSet()
{
	if (!m_RayTracingPipeline || !m_Scene)
		return;

	if (m_SceneDescriptorSet)
		m_ResourceAllocator->ReleaseResource(m_SceneDescriptorSet.get());
	m_SceneDescriptorSet = std::make_unique<Rdn::DescriptorSet>();
	m_ResourceAllocator->AllocateDescriptorSet(m_SceneDescriptorSet.get(), m_RayTracingPipeline.get(), 1);
	m_ResourceAllocator->UpdateDescriptorSet(m_SceneDescriptorSet.get(), m_Scene->GetDescriptorWrite(m_LinearSampler->GetHandle()));
}

bool PathTracer::CreatePipelines()
{
	const std::pair<const char*, std::unique_ptr<Rdn::Shader>*> shaders[] = {
		{ "res/Shaders/PathTracer/raygen.rgen", &m_RayGenShader },
		{ "res/Shaders/PathTracer/raymiss.rmiss", &m_RayMissShader },
		{ "res/Shaders/PathTracer/rayclosesthit.rchit", &m_RayClosestHitShader },
		{ "res/Shaders/PathTracer/shader.vert", &m_CompositeVertShader },
		{ "res/Shaders/PathTracer/shader.frag", &m_CompositeFragShader },
	};

	Rdn::ShaderDesc shaderDescs[std::size(shaders)];
	bool compiled = true;
	m_ShaderSources.clear();
	for (size_t i = 0; i < std::size(shaders); i++)
	{
		compiled &= Rdn::ShaderCompiler::Compile(shaders[i].first, shaderDescs[i]);
		m_ShaderSources.insert(m_ShaderSources.end(), shaderDescs[i].SourceFiles.begin(), shaderDescs[i].SourceFiles.end());
	}
	if (!compiled)
		return false;

	if (m_RayTracingPipeline)
	{
		m_Device->WaitIdle();
		m_ResourceAllocator->ReleaseResource(m_RayTracingPipeline.get());
		m_ResourceAllocator->ReleaseResource(m_CompositePipeline.get());
		for (const auto& shader : shaders)
			m_ResourceAllocator->ReleaseResource(shader.second->get());
	}

	for (size_t i = 0; i < std::size(shaders); i++)
	{
		*shaders[i].second = std::make_unique<Rdn::Shader>();
		m_ResourceAllocator->CreateShader(shaders[i].second->get(), shaderDescs[i]);
	}

	m_RayTracingPipeline = std::make_unique<Rdn::RayTracingPipeline>();
	m_ResourceAllocator->CreateRaytracingPipeline(m_RayTracingPipeline.get(), Rdn::RayTracingPipelineDesc{
		.ShaderStages = { m_RayGenShader.get(), m_RayMissShader.get(), m_RayClosestHitShader.get()},
		.PushDescriptorSets = { 0 }
	});

	m_CompositePipeline = std::make_unique<Rdn::Pipeline>();;
	m_ResourceAllocator->CreatePipeline(m_CompositePipeline.get(), Rdn::PipelineDesc{
		.ShaderStages = { m_CompositeVertShader.get(), m_CompositeFragShader.get() },
		.Topology = Rdn::PrimitiveTopology::TriangleFan,
		.ColorAttachmentFormats = { m_PresentQueue->GetSurfaceFormat() },
		.PushDescriptorSets = { 0 }
	});
	return true;
}

void PathTracer::CreateSamplers()
{
	m_NearestSampler = std::make_unique<Rdn::Sampler>();
	m_ResourceAllocator->CreateSampler(m_NearestSampler.get(), Rdn::SamplerDesc{
		.MinFilter = Rdn::Filter::Nearest,
		.MagFilter = Rdn::Filter::Nearest,
		.AddressMode = Rdn::SamplerAddressMode::Repeat
	});
	m_LinearSampler = std::make_unique<Rdn::Sampler>();
	m_ResourceAllocator->CreateSampler(m_LinearSampler.get(), Rdn::SamplerDesc{
		.MinFilter = Rdn::Filter::Linear,
		.MagFilter = Rdn::Filter::Linear,
		.AddressMode = Rdn::SamplerAddressMode::Repeat
	});
}

void PathTracer::CreateImages()
{
	Rdn::Extent2D extent = m_PresentQueue->GetExtent();

	m_Device->WaitIdle();
	
	if (m_OutputImage.get())
		m_ResourceAllocator->ReleaseResource(m_OutputImage.get());

	m_OutputImage = std::make_unique<Rdn::GpuImage>();
	auto imageDesc = Rdn::GpuImageDesc{
		.ImageSize = Rdn::Extent3D(extent),
		.Format = Rdn::Format::R8G8B8A8_Unorm,
		.UsageFlags = Rdn::ImageUsage::Storage | Rdn::ImageUsage::Sampled | Rdn::ImageUsage::TransferSrc,
		.AspectFlags = Rdn::ImageAspect::Color,
	};
	m_ResourceAllocator->CreateGpuImage(m_OutputImage.get(), imageDesc);

	if (m_AccumulationImage1.get())
		m_ResourceAllocator->ReleaseResource(m_AccumulationImage1.get());
	if (m_AccumulationImage2.get())
		m_ResourceAllocator->ReleaseResource(m_AccumulationImage2.get());
	m_AccumulationImage1 = std::make_unique<Rdn::GpuImage>();
	m_AccumulationImage2 = std::make_unique<Rdn::GpuImage>();
	{
		Rdn::GpuImageDesc desc = Rdn::GpuImageDesc{
		.ImageSize = Rdn::Extent3D(extent),
		.Format = Rdn::Format::R32G32B32A32_Sfloat,
		.UsageFlags = Rdn::ImageUsage::TransferDst | Rdn::ImageUsage::Storage,
		.AspectFlags = Rdn::ImageAspect::Color,
		.MipLevels = 1,
		};
		m_ResourceAllocator->CreateGpuImage(m_AccumulationImage1.get(), desc);
		m_ResourceAllocator->CreateGpuImage(m_AccumulationImage2.get(), desc);
	}
	m_ClearAccumulation = true; // done by the next frame's render graph

	m_Camera->AspectRatio = extent.Width / (float)extent.Height;
}

PathTracer::~PathTracer()
{

}

void PathTracer::RenderFrame(float elapsedTime)
{
	static float lastTime = 0.0f;
	float dt = elapsedTime - lastTime;
	lastTime = elapsedTime;

	if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::G))
	{
		std::vector<uint8_t> imageData = m_ResourceAllocator->GetImageData(m_OutputImage.get());
		std::string path = "img.png";
		stbi_write_png(path.c_str(), m_OutputImage->GetImageSize().Width, m_OutputImage->GetImageSize().Height, 4, imageData.data(), m_OutputImage->GetImageSize().Width * 4);
		RDN_LOG("Screenshot saved to disk");
	}
	if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F2))
		m_LogRenderGraph = true;
	if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F3))
	{
		RDN_LOG("{}", m_ResourceAllocator->DescribeMemoryUsage());
	}
	if (elapsedTime >= m_NextShaderCheckTime)
	{
		m_NextShaderCheckTime = elapsedTime + 0.5f;
		if (Rdn::ShaderCompiler::AnySourceChanged(m_ShaderSources))
			RecompileShaders();
	}

	//std::this_thread::sleep_for(std::chrono::milliseconds(16));
	if (!m_RayTracingPipeline)
		return;

	std::optional<Rdn::FrameContext> frameData = m_PresentQueue->BeginFrame();
	if (!frameData)
		return;
	m_IsEvenFrame = !m_IsEvenFrame; // only flip the accumulation ping-pong for frames that render

	m_Camera->Update(dt);

	globalFrameData.CameraPosition = glm::vec4(m_Camera->Position, 0);
	globalFrameData.CameraForward = glm::vec4(m_Camera->Forward, 0);
	globalFrameData.CameraRight = glm::vec4(m_Camera->Right, 0);
	globalFrameData.CameraUp = glm::vec4(-m_Camera->Up, 0);
	globalFrameData.TanHalfFov = glm::tan(glm::radians(m_Camera->Fov) * 0.5f);
	globalFrameData.AspectRatio = m_Camera->AspectRatio;
	globalFrameData.Aperture = 0.0;
	globalFrameData.FocusDistance = 1.0;
	globalFrameData.FrameIndex++;

	if (m_Camera->HasMoved || Rdn::Input::IsKeyDown(Rdn::KeyCode::C))
	{
		globalFrameData.FrameIndex = 1;
	}
	
	m_GlobalDataBuffer->Write(frameData->FrameIndex, globalFrameData);

	Rdn::RenderGraph& graph = *m_RenderGraph;
	graph.Reset();

	Rdn::RGImage backbuffer = graph.ImportBackbuffer(*frameData);
	Rdn::RGImage accumIn = graph.ImportImage("Accumulation in", m_IsEvenFrame ? m_AccumulationImage1.get() : m_AccumulationImage2.get());
	Rdn::RGImage accumOut = graph.ImportImage("Accumulation out", m_IsEvenFrame ? m_AccumulationImage2.get() : m_AccumulationImage1.get());
	Rdn::RGImage output = graph.ImportImage("Output", m_OutputImage.get());

	if (m_ClearAccumulation)
	{
		m_ClearAccumulation = false;
		graph.AddPass("Clear accumulation", Rdn::PassType::Transfer,
			[&](Rdn::PassBuilder& builder) {
				builder.UseImage(accumIn, Rdn::RGImageUsage::TransferDst);
				builder.UseImage(accumOut, Rdn::RGImageUsage::TransferDst);
			},
			[&](Rdn::CommandBuffer& cmd, const Rdn::PassContext& ctx) {
				const std::array<float, 4> clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
				cmd.ClearColorImage(ctx.GetImage(accumIn), clearColor, Rdn::ImageLayout::TransferDstOptimal);
				cmd.ClearColorImage(ctx.GetImage(accumOut), clearColor, Rdn::ImageLayout::TransferDstOptimal);
			}
		);
	}

	graph.AddPass("Trace rays", Rdn::PassType::RayTracing,
		[&](Rdn::PassBuilder& builder) {
			builder.UseImage(accumIn, Rdn::RGImageUsage::StorageRead);
			builder.UseImage(accumOut, Rdn::RGImageUsage::StorageWrite);
			builder.UseImage(output, Rdn::RGImageUsage::StorageWrite);
		},
		[&](Rdn::CommandBuffer& cmd, const Rdn::PassContext& ctx) {
			cmd.PushDescriptorSets(Rdn::PipelineBindPoint::RayTracing, m_RayTracingPipeline->GetLayout(), 0, Rdn::DescriptorWrite()
				.AddWrite(Rdn::AccelerationStructureWrite(0, Rdn::DescriptorType::AccelerationStructure, m_Scene->GetTopLevelAS()))
				.AddWrite(Rdn::ImageWrite(1, Rdn::DescriptorType::StorageImage, ctx.GetImageView(accumIn), Rdn::ImageLayout::General, {}))
				.AddWrite(Rdn::ImageWrite(2, Rdn::DescriptorType::StorageImage, ctx.GetImageView(accumOut), Rdn::ImageLayout::General, {}))
				.AddWrite(Rdn::ImageWrite(3, Rdn::DescriptorType::StorageImage, ctx.GetImageView(output), Rdn::ImageLayout::General, {}))
				.AddWrite(Rdn::BufferWrite(4, Rdn::DescriptorType::UniformBuffer, m_GlobalDataBuffer->GetHandle(), m_GlobalDataBuffer->GetOffset(ctx.FrameIndex), m_GlobalDataBuffer->GetElementSize())));
			cmd.BindDescriptorSets(Rdn::PipelineBindPoint::RayTracing, m_RayTracingPipeline->GetLayout(), 1, { m_SceneDescriptorSet->GetHandle() });

			cmd.BindRayTracingPipeline(m_RayTracingPipeline->GetHandle());
			Rdn::Extent3D size = ctx.GetImageExtent(output);
			cmd.TraceRays(m_RayTracingPipeline.get(), size.Width, size.Height, 1);
		}
	);

	graph.AddPass("Composite", Rdn::PassType::Graphics,
		[&](Rdn::PassBuilder& builder) {
			builder.UseImage(output, Rdn::RGImageUsage::Sampled, Rdn::ShaderStage::Fragment);
			builder.AddColorAttachment(backbuffer, Rdn::AttachmentLoadOp::Clear, Rdn::AttachmentStoreOp::Store, { 0.1f, 0.6f, 0.6f, 1.0f });
		},
		[&](Rdn::CommandBuffer& cmd, const Rdn::PassContext& ctx) {
			cmd.BindPipeline(m_CompositePipeline->GetHandle());
			cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_CompositePipeline->GetLayout(), 0, Rdn::DescriptorWrite()
				.AddWrite(Rdn::ImageWrite(0, Rdn::DescriptorType::CombinedImageSampler, ctx.GetImageView(output), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_NearestSampler->GetHandle())));
			cmd.Draw(4, 1, 0, 0);
		}
	);

	graph.Execute(frameData->Cmd, frameData->FrameIndex);
	if (m_LogRenderGraph)
	{
		RDN_LOG("{}", graph.DescribeCompiledFrame());
		m_LogRenderGraph = false;
	}
	m_PresentQueue->Submit();
}

void PathTracer::SwapchainResized()
{
	globalFrameData.FrameIndex = 0;
	CreateImages();
}

void PathTracer::RecompileShaders()
{
	const Rdn::Timer timer;
	if (!CreatePipelines())
	{
		RDN_LOG_ERROR("Shader reload failed{}", m_RayTracingPipeline ? ", keeping the previous shaders" : "");
		return;
	}

	CreateSceneDescriptorSet();
	m_ClearAccumulation = true;
	globalFrameData.FrameIndex = 0;
	RDN_LOG("Reloaded shaders in {:.0f} ms", timer.ElapsedMilliseconds());
}

void PathTracer::WaitForFrameEnd()
{

}
#include "PathTracer.h"
#include "RadianceEngine/Window/Input.h"
#include "RadianceEngine/Graphics/ShaderCompiler.h"
#include "RadianceEngine/Core/Log.h"
#include "stb/stb_image_write.h"
#include "stb/stb_image.h"
#include "tiny_obj_loader/tiny_obj_loader.h"
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
	m_Camera->Position = glm::vec3(0, 0.5f, -1.5f);
	m_Camera->MovementSpeed = 1.0f;

	CreateSamplers();
	CreatePipelines();
	CreateAccelerationStructure();
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

bool PathTracer::LoadObjModel(std::string filePath, std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices)
{
	tinyobj::ObjReader reader;
	tinyobj::ObjReaderConfig config;
	config.triangulate = true;
	config.mtl_search_path = "./";

	if (!reader.ParseFromFile(filePath, config)) {
		if (!reader.Error().empty()) {
			RDN_LOG_ERROR("TinyObjReader: {}", reader.Error());
		}
		return false;
	}

	if (!reader.Warning().empty()) {
		RDN_LOG_WARNING("TinyObjReader: {}", reader.Warning());
	}

	const auto& attrib = reader.GetAttrib();
	const auto& shapes = reader.GetShapes();

	std::unordered_map<Vertex, uint32_t, VertexHasher> uniqueVertices;

	for (const auto& shape : shapes) {
		size_t indexOffset = 0;
		for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
			int fv = shape.mesh.num_face_vertices[f];

			for (int v = 0; v < fv; ++v) {
				tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];

				Vertex vertex{};

				vertex.pos = {
					attrib.vertices[3 * idx.vertex_index + 0],
					attrib.vertices[3 * idx.vertex_index + 1],
					attrib.vertices[3 * idx.vertex_index + 2]
				};

				if (idx.normal_index >= 0) {
					vertex.normal = {
						attrib.normals[3 * idx.normal_index + 0],
						attrib.normals[3 * idx.normal_index + 1],
						attrib.normals[3 * idx.normal_index + 2]
					};
				}
				else {
					vertex.normal = { 0.0f, 0.0f, 0.0f };
				}

				if (idx.texcoord_index >= 0) {
					vertex.uv = {
						attrib.texcoords[2 * idx.texcoord_index + 0],
						1.0f - attrib.texcoords[2 * idx.texcoord_index + 1]
					};
				}
				else {
					vertex.uv = { 0.0f, 0.0f };
				}

				if (uniqueVertices.count(vertex) == 0) {
					uniqueVertices[vertex] = static_cast<uint32_t>(outVertices.size());
					outVertices.push_back(vertex);
				}

				outIndices.push_back(uniqueVertices[vertex]);
			}

			indexOffset += fv;
		}
	}

	RDN_LOG("Loaded OBJ model: {} vertices, {} indices", outVertices.size(), outIndices.size());
	return true;
}

void PathTracer::LoadModel(std::string filepath, Model* model)
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	if (!LoadObjModel(filepath.c_str(), vertices, indices)) {
		RDN_LOG_ERROR("Failed to load OBJ! {}", filepath.c_str());
		return;
	}

	m_ResourceAllocator->CreateGpuBuffer(&model->VertexBuffer, Rdn::GpuBufferDesc{
		.Size = (uint32_t)(vertices.size() * sizeof(Vertex)),
		.UsageFlags = Rdn::BufferUsage::StorageBuffer | Rdn::BufferUsage::ShaderDeviceAddress | Rdn::BufferUsage::AccelerationStructureBuildInputReadOnly | Rdn::BufferUsage::TransferDst,
		.MemoryProperty = Rdn::MemoryProperty::DeviceLocal,
		});
	m_ResourceAllocator->CreateGpuBuffer(&model->IndexBuffer, Rdn::GpuBufferDesc{
		.Size = (uint32_t)(indices.size() * sizeof(uint32_t)),
		.UsageFlags = Rdn::BufferUsage::StorageBuffer | Rdn::BufferUsage::ShaderDeviceAddress | Rdn::BufferUsage::AccelerationStructureBuildInputReadOnly | Rdn::BufferUsage::TransferDst,
		.MemoryProperty = Rdn::MemoryProperty::DeviceLocal,
		});

	m_ResourceAllocator->SetDeviceLocalBufferData(&model->VertexBuffer, vertices.data(), vertices.size() * sizeof(Vertex));
	m_ResourceAllocator->SetDeviceLocalBufferData(&model->IndexBuffer, indices.data(), indices.size() * sizeof(uint32_t));

	m_ResourceAllocator->CreateBottomLevelAS(&model->BottomLevelAS, Rdn::BottomLevelASDesc{
		.VertexBuffer = model->VertexBuffer.GetHandle(),
		.IndexBuffer = model->IndexBuffer.GetHandle(),
		.VertexStride = sizeof(Vertex),
		.VertexCount = (uint32_t)vertices.size(),
		.NumTriangles = (uint32_t)indices.size() / 3,
		.VertexFormat = Rdn::Format::R32G32B32_Sfloat
	});

	model->VertexBufferDeviceAddress = m_ResourceAllocator->GetBufferDeviceAddress(&model->VertexBuffer);
	model->IndexBufferDeviceAddress = m_ResourceAllocator->GetBufferDeviceAddress(&model->IndexBuffer);
}

void PathTracer::AddInstance(std::vector<MeshPrimitive>& meshInfos, std::vector<Material>& materials, Model* model, glm::mat4 transform, const Material& material)
{
	m_Scene->AddInstance(Rdn::TopLevelASInstance{
		.BottomLevelASHandle = model->BottomLevelAS.GetHandle(),
		.InstanceCustomIndex = (uint32_t)meshInfos.size(),
		.TransformMatrix = {
			transform[0][0], transform[1][0], transform[2][0], transform[3][0],
			transform[0][1], transform[1][1], transform[2][1], transform[3][1],
			transform[0][2], transform[1][2], transform[2][2], transform[3][2],
		}
	});

	MeshPrimitive meshInfo{};
	meshInfo.VertexBufferDeviceAddress = model->VertexBufferDeviceAddress;
	meshInfo.IndexBufferDeviceAddress = model->IndexBufferDeviceAddress;

	uint32_t materialIndex = 0xFFFFFFFF;
	for (uint32_t i = 0; i < materials.size(); ++i)
	{
		if (materials[i] == material)
		{
			materialIndex = i;
			break;
		}
	}

	if (materialIndex == 0xFFFFFFFF)
	{
		materialIndex = static_cast<uint32_t>(materials.size());
		materials.push_back(material);
	}
	meshInfo.MaterialIndex = materialIndex;
	meshInfos.push_back(meshInfo);
}

void PathTracer::CreateAccelerationStructure()
{
	m_Plane = std::make_unique<Model>();
	LoadModel("res/3DModels/plane.obj", m_Plane.get());
	m_Dragon = std::make_unique<Model>();
	LoadModel("res/3DModels/dragon.obj", m_Dragon.get());
	//m_Sphere = std::make_unique<Model>();
	//LoadModel("res/3DModels/sphere.obj", m_Sphere.get());
	m_Scene = std::make_unique<Rdn::TopLevelAS>();
	m_ResourceAllocator->CreateTopLevelAS(m_Scene.get());

	std::vector<MeshPrimitive> meshInfos;
	std::vector<Material> materials;
	Material glass{};
	glass.Color = glm::vec4(0.95, 0.7, 0.7, 1);
	glass.Roughness = 0.1f;
	glass.Transmission = 1.0f;
	Material redWall{};
	redWall.Color = vec4(1.0, 0.2, 0.3, 1);
	redWall.TextureIndex = 1;
	Material greenWall{};
	greenWall.Color = vec4(0.25, 1.0, 0.35, 1);
	greenWall.Metallic = 1.0f;
	Material blueWall{};
	blueWall.Color = vec4(0.25, 0.55, 1.0, 1);
	Material whiteWall{};
	whiteWall.Color = vec4(0.8, 0.8, 0.8, 1);
	Material light{};
	light.Color = vec4(1);
	light.Emission = vec4(15.0, 15.0, 15.0, 1);

	{ // FLOOR
		mat4 T = translate(mat4(1.0f), vec3(0.0f, 0.0f, 0.0f));
		mat4 R = mat4(1.0f);
		mat4 S = scale(mat4(1.0f), vec3(1.0f, 1.0f, 1.0f));
		AddInstance(meshInfos, materials, m_Plane.get(), T * R * S, whiteWall);
	}
	{ // CEILING
		mat4 T = translate(mat4(1.0f), vec3(0.0f, 1.0f, 0.0f));
		mat4 R = rotate(mat4(1.0f), radians(180.0f), vec3(1, 0, 0));
		mat4 S = scale(mat4(1.0f), vec3(1.0f));
		AddInstance(meshInfos, materials, m_Plane.get(), T * R * S, whiteWall);
	}
	{ // BACK
		mat4 T = translate(mat4(1.0f), vec3(0.0f, 0.5f, 0.5f));
		mat4 R = rotate(mat4(1.0f), radians(-90.0f), vec3(1, 0, 0));
		mat4 S = scale(mat4(1.0f), vec3(1.0f));
		AddInstance(meshInfos, materials, m_Plane.get(), T * R * S, blueWall);
	}
	{ // LEFT
		mat4 T = translate(mat4(1.0f), vec3(-0.5f, 0.5f, 0.0f));
		mat4 R = rotate(mat4(1.0f), radians(-90.0f), vec3(0, 0, 1));
		mat4 S = scale(mat4(1.0f), vec3(1.0f));
		AddInstance(meshInfos, materials, m_Plane.get(), T * R * S, redWall);
	}
	{ // RIGHT
		mat4 T = translate(mat4(1.0f), vec3(+0.5f, 0.5f, 0.0f));
		mat4 R = rotate(mat4(1.0f), radians(90.0f), vec3(0, 0, 1));
		mat4 S = scale(mat4(1.0f), vec3(1.0f));
		AddInstance(meshInfos, materials, m_Plane.get(), T * R * S, greenWall);
	}
	{ // LIGHT
		mat4 T = translate(mat4(1.0f), vec3(0.0f, 0.999f, 0.0f));
		mat4 R = rotate(mat4(1.0f), radians(180.0f), vec3(1, 0, 0));
		mat4 S = scale(mat4(1.0f), vec3(0.25f));
		AddInstance(meshInfos, materials, m_Plane.get(), T * R * S, light);
	}
	{ // DRAGON
		mat4 T = translate(mat4(1.0f), vec3(0.0f, 0.26f, 0.0f));
		mat4 R = rotate(mat4(1.0f), radians(70.0f), vec3(0, 1, 0));
		mat4 S = scale(mat4(1.0f), vec3(0.9f));
		AddInstance(meshInfos, materials, m_Dragon.get(), T * R * S, glass);
	}
	//{ // SPHERE
	//	mat4 T = translate(mat4(1.0f), vec3(0.0f, 0.7f, 0.0f));
	//	mat4 R = mat4(1.0f);
	//	mat4 S = scale(mat4(1.0f), vec3(0.3f));
	//	AddInstance(meshInfos, materials, m_Sphere.get(), T * R * S, glass);
	//}
	m_ResourceAllocator->BuildTopLevelAS(m_Scene.get());

	m_MeshInfoBuffer = std::make_unique<Rdn::GpuBuffer>();
	m_ResourceAllocator->CreateGpuBuffer(m_MeshInfoBuffer.get(), Rdn::GpuBufferDesc{
		.Size = (uint32_t)meshInfos.size() * (uint32_t)sizeof(MeshPrimitive),
		.UsageFlags = Rdn::BufferUsage::StorageBuffer | Rdn::BufferUsage::ShaderDeviceAddress,
		.MemoryProperty = Rdn::MemoryProperty::HostVisible
	});

	void* data = m_ResourceAllocator->MapMemory(m_MeshInfoBuffer.get());
	memcpy(data, meshInfos.data(), meshInfos.size() * sizeof(MeshPrimitive));
	m_ResourceAllocator->UnmapMemory(m_MeshInfoBuffer.get());

	m_MaterialBuffer = std::make_unique<Rdn::GpuBuffer>();
	m_ResourceAllocator->CreateGpuBuffer(m_MaterialBuffer.get(), Rdn::GpuBufferDesc{
		.Size = (uint32_t)materials.size() * (uint32_t)sizeof(Material),
		.UsageFlags = Rdn::BufferUsage::StorageBuffer | Rdn::BufferUsage::ShaderDeviceAddress,
		.MemoryProperty = Rdn::MemoryProperty::HostVisible
		});

	data = m_ResourceAllocator->MapMemory(m_MaterialBuffer.get());
	memcpy(data, materials.data(), materials.size() * sizeof(Material));
	m_ResourceAllocator->UnmapMemory(m_MaterialBuffer.get());


	m_WhiteTexture = std::make_unique<Rdn::GpuImage>();
	m_ResourceAllocator->CreateGpuImage(m_WhiteTexture.get(), Rdn::GpuImageDesc{
		.ImageSize = { 1, 1, 1},
		.Format = Rdn::Format::R8G8B8A8_Unorm,
		.UsageFlags = Rdn::ImageUsage::TransferDst | Rdn::ImageUsage::Sampled,
		.AspectFlags = Rdn::ImageAspect::Color,
		});
	uint32_t whitePixel = 0xFFFFFFFF;
	m_ResourceAllocator->SetImageData(m_WhiteTexture.get(), &whitePixel, sizeof(uint32_t), Rdn::ImageLayout::ShaderReadOnlyOptimal);

	int imgWidth, imgHeight, imgChannels;
	const char* file = "res/Images/Granite.jpg";
	stbi_info(file, &imgWidth, &imgHeight, &imgChannels);
	m_Texture = std::make_unique<Rdn::GpuImage>();
	m_ResourceAllocator->CreateGpuImage(m_Texture.get(), Rdn::GpuImageDesc{
		.ImageSize = { (uint32_t)imgWidth, (uint32_t)imgHeight, 1},
		.Format = Rdn::Format::R8G8B8A8_Unorm,
		.UsageFlags = Rdn::ImageUsage::TransferDst | Rdn::ImageUsage::Sampled,
		.AspectFlags = Rdn::ImageAspect::Color,
		});
	data = stbi_load(file, &imgWidth, &imgHeight, &imgChannels, 4);
	m_ResourceAllocator->SetImageData(m_Texture.get(), data, imgWidth * imgHeight * 4 * sizeof(uint8_t), Rdn::ImageLayout::ShaderReadOnlyOptimal);
	stbi_image_free(data);


	m_SceneDescriptorSet = std::make_unique<Rdn::DescriptorSet>();
	m_ResourceAllocator->AllocateDescriptorSet(m_SceneDescriptorSet.get(), m_RayTracingPipeline.get(), 1);
	m_ResourceAllocator->UpdateDescriptorSet(m_SceneDescriptorSet.get(), Rdn::DescriptorWrite()
		.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_MeshInfoBuffer->GetHandle()))
		.AddWrite(Rdn::BufferWrite(1, Rdn::DescriptorType::StorageBuffer, m_MaterialBuffer->GetHandle()))
		.AddWrite(Rdn::ImageWrite(2, Rdn::DescriptorType::CombinedImageSampler, m_WhiteTexture->GetImageView(), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_LinearSampler->GetHandle(), 0))
		.AddWrite(Rdn::ImageWrite(2, Rdn::DescriptorType::CombinedImageSampler, m_Texture->GetImageView(), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_LinearSampler->GetHandle(), 1))
	);
	//m_Device->UpdateDescriptorSet(m_SceneDescriptorSet.get(), Rdn::DescriptorWrite()
	//	.AddWrite(Rdn::BufferWrite(0, m_MeshInfoBuffer.get()))
	//	.AddWrite(Rdn::BufferWrite(1, m_MaterialBuffer.get()))
	//	.AddWrite(Rdn::ImageWrite(2, 0, m_WhiteTexture.get(), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_LinearSampler.get()))
	//	.AddWrite(Rdn::ImageWrite(2, 1, m_Texture.get(), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_LinearSampler.get()))
	//);
}

void PathTracer::CreatePipelines()
{
	m_RayGenShader = std::make_unique<Rdn::Shader>();
	m_RayMissShader = std::make_unique<Rdn::Shader>();
	m_RayClosestHitShader = std::make_unique<Rdn::Shader>();
	m_CompositeVertShader = std::make_unique<Rdn::Shader>();
	m_CompositeFragShader = std::make_unique<Rdn::Shader>();

	Rdn::ShaderDesc shaderDesc;
	if (Rdn::ShaderCompiler::Compile("res/Shaders/PathTracer/raygen.rgen", shaderDesc))
		m_ResourceAllocator->CreateShader(m_RayGenShader.get(), shaderDesc);
	if (Rdn::ShaderCompiler::Compile("res/Shaders/PathTracer/raymiss.rmiss", shaderDesc))
		m_ResourceAllocator->CreateShader(m_RayMissShader.get(), shaderDesc);
	if (Rdn::ShaderCompiler::Compile("res/Shaders/PathTracer/rayclosesthit.rchit", shaderDesc))
		m_ResourceAllocator->CreateShader(m_RayClosestHitShader.get(), shaderDesc);
	if (Rdn::ShaderCompiler::Compile("res/Shaders/PathTracer/shader.vert", shaderDesc))
		m_ResourceAllocator->CreateShader(m_CompositeVertShader.get(), shaderDesc);
	if (Rdn::ShaderCompiler::Compile("res/Shaders/PathTracer/shader.frag", shaderDesc))
		m_ResourceAllocator->CreateShader(m_CompositeFragShader.get(), shaderDesc);

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
		const float clearColor[4] = { 0.0, 0.0, 0.0, 0.0 };
		m_Device->ImmediateSubmit([&](Rdn::CommandBuffer& cmd) {
			cmd.TransitionImage(m_AccumulationImage1->GetHandle(),
				Rdn::ImageLayout::Undefined,
				Rdn::ImageLayout::General,
				Rdn::PipelineStage::None,
				Rdn::PipelineStage::ComputeShader,
				Rdn::ImageAspect::Color);
			cmd.TransitionImage(m_AccumulationImage2->GetHandle(),
				Rdn::ImageLayout::Undefined,
				Rdn::ImageLayout::General,
				Rdn::PipelineStage::None,
				Rdn::PipelineStage::ComputeShader,
				Rdn::ImageAspect::Color);

			cmd.ClearColorImage(m_AccumulationImage1->GetHandle(), clearColor, Rdn::ImageLayout::General);
			cmd.ClearColorImage(m_AccumulationImage2->GetHandle(), clearColor, Rdn::ImageLayout::General);
		});
	}
	m_Device->WaitIdle();

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
	m_IsEvenFrame = !m_IsEvenFrame;

	if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::G))
	{
		std::vector<uint8_t> imageData = m_ResourceAllocator->GetImageData(m_OutputImage.get(), Rdn::ImageLayout::ShaderReadOnlyOptimal);
		std::string path = "img.png";
		stbi_write_png(path.c_str(), m_OutputImage->GetImageSize().Width, m_OutputImage->GetImageSize().Height, 4, imageData.data(), m_OutputImage->GetImageSize().Width * 4);
		RDN_LOG("Screenshot saved to disk");
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(16));

	Rdn::FrameContext frameData = m_PresentQueue->BeginFrame();
	Rdn::CommandBuffer* cmd = &frameData.Cmd;

	m_Camera->Update(dt);

	globalFrameData.CameraPosition = glm::vec4(m_Camera->Position, 0);
	globalFrameData.CameraForward = glm::vec4(m_Camera->Forward, 0);
	globalFrameData.CameraRight = glm::vec4(m_Camera->Right, 0);
	globalFrameData.CameraUp = glm::vec4(-m_Camera->Up, 0);
	globalFrameData.TanHalfFov = glm::tan(m_Camera->Fov * 0.5);
	globalFrameData.AspectRatio = m_Camera->AspectRatio;
	globalFrameData.Aperture = 0.0;
	globalFrameData.FocusDistance = 1.0;
	globalFrameData.FrameIndex++;

	if (m_Camera->HasMoved || Rdn::Input::IsKeyDown(Rdn::KeyCode::C))
	{
		globalFrameData.FrameIndex = 1;
	}
	
	m_GlobalDataBuffer->Write(frameData.FrameIndex, globalFrameData);

	m_RenderGraph->Reset();

	Rdn::GpuImage* accumImageIn = m_IsEvenFrame ? m_AccumulationImage1.get() : m_AccumulationImage2.get();
	Rdn::GpuImage* accumImageOut = m_IsEvenFrame ? m_AccumulationImage2.get() : m_AccumulationImage1.get();

	m_RenderGraph->AddPass("Trace rays",
		[&](Rdn::PassBuilder& builder) {
			builder.UseImage(accumImageIn->GetHandle(), Rdn::ImageLayout::General, Rdn::PipelineStage::RayTracingShader, Rdn::AccessMask::ShaderStorageRead);
			builder.UseImage(accumImageOut->GetHandle(), Rdn::ImageLayout::General, Rdn::PipelineStage::RayTracingShader, Rdn::AccessMask::ShaderStorageWrite);
			builder.UseImage(m_OutputImage->GetHandle(), Rdn::ImageLayout::General, Rdn::PipelineStage::RayTracingShader, Rdn::AccessMask::ShaderStorageWrite);
		},
		[&](Rdn::CommandBuffer& cmd, uint32_t frameIndex) {
			cmd.PushDescriptorSets(Rdn::PipelineBindPoint::RayTracing, m_RayTracingPipeline->GetLayout(), 0, Rdn::DescriptorWrite()
				.AddWrite(Rdn::AccelerationStructureWrite(0, Rdn::DescriptorType::AccelerationStructure, m_Scene->GetHandle()))
				.AddWrite(Rdn::ImageWrite(1, Rdn::DescriptorType::StorageImage, accumImageIn->GetImageView(), Rdn::ImageLayout::General, {}))
				.AddWrite(Rdn::ImageWrite(2, Rdn::DescriptorType::StorageImage, accumImageOut->GetImageView(), Rdn::ImageLayout::General, {}))
				.AddWrite(Rdn::ImageWrite(3, Rdn::DescriptorType::StorageImage, m_OutputImage->GetImageView(), Rdn::ImageLayout::General, {}))
				.AddWrite(Rdn::BufferWrite(4, Rdn::DescriptorType::UniformBuffer, m_GlobalDataBuffer->GetHandle(), m_GlobalDataBuffer->GetOffset(frameIndex), m_GlobalDataBuffer->GetElementSize())));
			cmd.BindDescriptorSets(Rdn::PipelineBindPoint::RayTracing, m_RayTracingPipeline->GetLayout(), 1, { m_SceneDescriptorSet->GetHandle() });

			cmd.BindRayTracingPipeline(m_RayTracingPipeline->GetHandle());
			cmd.TraceRays(m_RayTracingPipeline.get(), m_OutputImage->GetImageSize().Width, m_OutputImage->GetImageSize().Height, 1);
		}
	);

	m_RenderGraph->SetPresentPass("Present",
		[&](Rdn::PassBuilder& builder) {
			builder.UseImage(m_OutputImage->GetHandle(), Rdn::ImageLayout::ShaderReadOnlyOptimal, Rdn::PipelineStage::FragmentShader, Rdn::AccessMask::ShaderSampledRead);
		},
		Rdn::AttachmentLoadOp::Clear, 0.1f, 0.6f, 0.6f, 1.0f,
		[&](Rdn::CommandBuffer& cmd, uint32_t frameIndex) {
			cmd.BindPipeline(m_CompositePipeline->GetHandle());
			cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_CompositePipeline->GetLayout(), 0, Rdn::DescriptorWrite()
				.AddWrite(Rdn::ImageWrite(0, Rdn::DescriptorType::CombinedImageSampler, m_OutputImage->GetImageView(), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_NearestSampler->GetHandle())));
			cmd.Draw(6, 1, 0, 0);
		}
	);

	m_RenderGraph->Execute(frameData.Cmd, frameData.FrameIndex, frameData.PresentImage, frameData.PresentImageView, m_PresentQueue->GetExtent());
	m_PresentQueue->Submit();
}

void PathTracer::SwapchainResized(void* presentQueue)
{
	m_PresentQueue = static_cast<Rdn::PresentQueue*>(presentQueue);
	globalFrameData.FrameIndex = 0;
	CreateImages();
}

void PathTracer::RecompileShaders()
{

}

void PathTracer::WaitForFrameEnd()
{

}
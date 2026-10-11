#include "WorldRenderer.h"
#include "Rendering/Effects.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Graphics/ShaderCompiler.h>
#include <glm/gtc/packing.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <tuple>
#include <utility>

namespace Game
{
	namespace
	{
		constexpr uint32_t RoundSegments = 16;
		constexpr uint32_t SphereStacks = 8;
		constexpr uint32_t SphereSlices = 12;
		constexpr float TwoPi = 6.28318531f;

		struct MeshVertex
		{
			glm::vec4 Position;
			glm::vec4 Normal;
		};

		using Mesh = std::vector<MeshVertex>;

		void Triangle(Mesh& mesh, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 na, glm::vec3 nb, glm::vec3 nc)
		{
			mesh.push_back({ glm::vec4(a, 1.0f), glm::vec4(na, 0.0f) });
			mesh.push_back({ glm::vec4(b, 1.0f), glm::vec4(nb, 0.0f) });
			mesh.push_back({ glm::vec4(c, 1.0f), glm::vec4(nc, 0.0f) });
		}

		void Triangle(Mesh& mesh, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 normal)
		{
			Triangle(mesh, a, b, c, normal, normal, normal);
		}

		void Quad(Mesh& mesh, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 normal)
		{
			Triangle(mesh, a, b, c, normal);
			Triangle(mesh, a, c, d, normal);
		}

		Mesh MakeBox()
		{
			Mesh mesh;
			for (const glm::vec3 normal : { glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, -1, 0), glm::vec3(0, 0, 1), glm::vec3(0, 0, -1) })
			{
				const glm::vec3 tangent = normal.y != 0.0f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
				const glm::vec3 bitangent = glm::cross(normal, tangent);
				Quad(mesh, normal - tangent - bitangent, normal + tangent - bitangent, normal + tangent + bitangent, normal - tangent + bitangent, normal);
			}
			return mesh;
		}

		Mesh MakeCylinder()
		{
			Mesh mesh;
			for (uint32_t i = 0; i < RoundSegments; i++)
			{
				const float a0 = TwoPi * float(i) / float(RoundSegments);
				const float a1 = TwoPi * float(i + 1) / float(RoundSegments);
				const glm::vec3 n0(std::cos(a0), 0.0f, std::sin(a0));
				const glm::vec3 n1(std::cos(a1), 0.0f, std::sin(a1));
				const glm::vec3 down(0.0f, -1.0f, 0.0f);
				const glm::vec3 up(0.0f, 1.0f, 0.0f);
				Triangle(mesh, n0 + down, n1 + down, n1 + up, n0, n1, n1);
				Triangle(mesh, n0 + down, n1 + up, n0 + up, n0, n1, n0);
				Triangle(mesh, up, n0 + up, n1 + up, up);
				Triangle(mesh, down, n1 + down, n0 + down, down);
			}
			return mesh;
		}

		Mesh MakeCylinderX()
		{
			Mesh mesh = MakeCylinder();
			for (MeshVertex& vertex : mesh)
			{
				std::swap(vertex.Position.x, vertex.Position.y);
				std::swap(vertex.Normal.x, vertex.Normal.y);
			}
			return mesh;
		}

		Mesh MakeCone()
		{
			Mesh mesh;
			const glm::vec3 apex(0.0f, 1.0f, 0.0f);
			const glm::vec3 down(0.0f, -1.0f, 0.0f);
			const auto sideNormal = [](float angle) { return glm::normalize(glm::vec3(2.0f * std::cos(angle), 1.0f, 2.0f * std::sin(angle))); };
			for (uint32_t i = 0; i < RoundSegments; i++)
			{
				const float a0 = TwoPi * float(i) / float(RoundSegments);
				const float a1 = TwoPi * float(i + 1) / float(RoundSegments);
				const glm::vec3 p0(std::cos(a0), -1.0f, std::sin(a0));
				const glm::vec3 p1(std::cos(a1), -1.0f, std::sin(a1));
				Triangle(mesh, p0, p1, apex, sideNormal(a0), sideNormal(a1), sideNormal(0.5f * (a0 + a1)));
				Triangle(mesh, down, p1, p0, down);
			}
			return mesh;
		}

		Mesh MakeSphere()
		{
			Mesh mesh;
			const auto point = [](uint32_t stack, uint32_t slice)
			{
				const float latitude = -0.5f * 3.14159265f + 3.14159265f * float(stack) / float(SphereStacks);
				const float longitude = TwoPi * float(slice) / float(SphereSlices);
				return glm::vec3(std::cos(latitude) * std::cos(longitude), std::sin(latitude), std::cos(latitude) * std::sin(longitude));
			};
			for (uint32_t stack = 0; stack < SphereStacks; stack++)
				for (uint32_t slice = 0; slice < SphereSlices; slice++)
				{
					const glm::vec3 a = point(stack, slice);
					const glm::vec3 b = point(stack, slice + 1);
					const glm::vec3 c = point(stack + 1, slice + 1);
					const glm::vec3 d = point(stack + 1, slice);
					Triangle(mesh, a, b, c, a, b, c);
					Triangle(mesh, a, c, d, a, c, d);
				}
			return mesh;
		}

		Mesh MakeWedge()
		{
			Mesh mesh;
			Quad(mesh, { -1, -1, -1 }, { 1, -1, -1 }, { 1, -1, 1 }, { -1, -1, 1 }, { 0, -1, 0 });
			Quad(mesh, { -1, -1, 1 }, { 1, -1, 1 }, { 1, 1, 1 }, { -1, 1, 1 }, { 0, 0, 1 });
			Quad(mesh, { -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, 1 }, { -1, 1, 1 }, glm::normalize(glm::vec3(0, 1, -1)));
			Triangle(mesh, { -1, -1, -1 }, { -1, -1, 1 }, { -1, 1, 1 }, { -1, 0, 0 });
			Triangle(mesh, { 1, -1, -1 }, { 1, 1, 1 }, { 1, -1, 1 }, { 1, 0, 0 });
			return mesh;
		}

		Mesh MakeRoof()
		{
			Mesh mesh;
			Quad(mesh, { -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, 0 }, { -1, 1, 0 }, glm::normalize(glm::vec3(0, 1, -2)));
			Quad(mesh, { -1, -1, 1 }, { 1, -1, 1 }, { 1, 1, 0 }, { -1, 1, 0 }, glm::normalize(glm::vec3(0, 1, 2)));
			Triangle(mesh, { -1, -1, -1 }, { -1, -1, 1 }, { -1, 1, 0 }, { -1, 0, 0 });
			Triangle(mesh, { 1, -1, -1 }, { 1, 1, 0 }, { 1, -1, 1 }, { 1, 0, 0 });
			Quad(mesh, { -1, -1, -1 }, { 1, -1, -1 }, { 1, -1, 1 }, { -1, -1, 1 }, { 0, -1, 0 });
			return mesh;
		}
	}

	WorldRenderer::WorldRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator, Rdn::Format colorFormat)
		: m_Device(device), m_Allocator(allocator)
	{
		if (!CreatePipeline() || !CreateModelPipeline())
			RDN_LOG_ERROR("The world shaders failed to compile, the world won't be drawn");
		CreateShadowPipelines();
		m_Particles = std::make_unique<ParticleSystem>(device, allocator);
		m_Particles->SetMaterials(MakeParticleMaterials());
		m_Deferred = std::make_unique<DeferredLighting>(device, allocator);
		m_Gi = std::make_unique<RadianceCascades>(device, allocator);
		m_Post = std::make_unique<PostProcess>(device, allocator, colorFormat);
		CreateMeshes();
		EnsureCapacity(16384);
		EnsureModelCapacity(1024, 4096);
		m_ModelSampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_ModelSampler.get(), Rdn::SamplerDesc{ .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
		m_NearestSampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_NearestSampler.get(), Rdn::SamplerDesc{ .MinFilter = Rdn::Filter::Nearest, .MagFilter = Rdn::Filter::Nearest, .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
		m_RepeatSampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_RepeatSampler.get(), Rdn::SamplerDesc{ .AddressMode = Rdn::SamplerAddressMode::Repeat, .MaxAnisotropy = 8.0f });
		m_EnvironmentSampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_EnvironmentSampler.get(), Rdn::SamplerDesc{ .AddressMode = Rdn::SamplerAddressMode::Repeat, .AddressModeV = Rdn::SamplerAddressMode::ClampToEdge });
		const auto create = [&](Rdn::Format format, const void* pixel, uint64_t size)
		{
			auto image = std::make_unique<Rdn::GpuImage>();
			m_Allocator.CreateGpuImage(image.get(), Rdn::GpuImageDesc{ .ImageSize = { 1, 1, 1 }, .Format = format, .UsageFlags = Rdn::ImageUsage::Sampled | Rdn::ImageUsage::TransferDst });
			m_Allocator.SetImageData(image.get(), pixel, size, Rdn::ImageLayout::ShaderReadOnlyOptimal);
			return image;
		};
		const uint8_t open = 255;
		m_SkyMask = create(Rdn::Format::R8_Unorm, &open, 1);
		const uint16_t black[4] = { 0, 0, 0, 0x3C00 };
		m_BlackEnvironment = create(Rdn::Format::R16G16B16A16_Sfloat, black, sizeof(black));
	}

	WorldRenderer::~WorldRenderer()
	{
		m_Device.WaitIdle();
		m_Particles.reset();
		m_Deferred.reset();
		m_Gi.reset();
		m_Post.reset();
		ReleaseModelData();
		for (Rdn::GpuRingBuffer* buffer : { m_InstanceBuffer.get(), m_ModelInstanceBuffer.get(), m_PaletteBuffer.get() })
			if (buffer)
				m_Allocator.ReleaseResource(buffer);
		if (m_MeshBuffer)
			m_Allocator.ReleaseResource(m_MeshBuffer.get());
		for (Rdn::GpuImage* image : { m_SkyMask.get(), m_BlackEnvironment.get() })
			if (image)
				m_Allocator.ReleaseResource(image);
		for (Rdn::Sampler* sampler : { m_ModelSampler.get(), m_NearestSampler.get(), m_RepeatSampler.get(), m_EnvironmentSampler.get() })
			if (sampler)
				m_Allocator.ReleaseResource(sampler);
		for (Rdn::Pipeline* pipeline : { m_ShadowPipeline.get(), m_ModelShadowPipeline.get(), m_ModelAlphaShadowPipeline.get(), m_BackPipeline.get(), m_ModelBackPipeline.get() })
			if (pipeline)
				m_Allocator.ReleaseResource(pipeline);
		for (Rdn::Shader* shader : { m_ModelShadowShader.get(), m_ModelBackShader.get() })
			if (shader)
				m_Allocator.ReleaseResource(shader);
		for (const auto& [pipeline, vertex, fragment] : { std::tuple(m_Pipeline.get(), m_VertexShader.get(), m_FragmentShader.get()), std::tuple(m_ModelPipeline.get(), m_ModelVertexShader.get(), m_ModelFragmentShader.get()) })
		{
			if (!pipeline)
				continue;
			m_Allocator.ReleaseResource(pipeline);
			m_Allocator.ReleaseResource(vertex);
			m_Allocator.ReleaseResource(fragment);
		}
	}

	void WorldRenderer::ReleaseModelData()
	{
		for (Rdn::GpuBuffer* buffer : { m_ModelVertices.get(), m_ModelIndices.get(), m_MaterialBuffer.get() })
			if (buffer)
				m_Allocator.ReleaseResource(buffer);
		for (Rdn::DescriptorSet* set : { m_TextureSet.get(), m_ShadowTextureSet.get(), m_BackTextureSet.get() })
			if (set)
				m_Allocator.ReleaseResource(set);
		for (const std::unique_ptr<Rdn::GpuImage>& texture : m_Textures)
			m_Allocator.ReleaseResource(texture.get());
		for (const EnvironmentGpu& environment : m_Environments)
			m_Allocator.ReleaseResource(environment.Image.get());
		m_ModelVertices.reset();
		m_ModelIndices.reset();
		m_MaterialBuffer.reset();
		m_TextureSet.reset();
		m_ShadowTextureSet.reset();
		m_BackTextureSet.reset();
		m_Textures.clear();
		m_TextureSamplers.clear();
		m_Environments.clear();
		m_DrawMeshes.clear();
		m_Materials.clear();
	}

	bool WorldRenderer::LoadModels(std::span<const ModelPack> packs, std::string& error)
	{
		if (!m_Models.Load(packs, error))
			return false;
		const ModelLibraryData& data = m_Models.GetData();
		if (data.Vertices.empty() || data.Indices.empty())
		{
			error = "the model library has no geometry";
			return false;
		}
		const bool compressed = std::any_of(data.Textures.begin(), data.Textures.end(), [](const ModelTexture& texture) { return texture.Format != TextureFormat::Rgba8 && texture.Format != TextureFormat::Rgba8Srgb; });
		if (compressed && !m_Device.GetProperties().TextureCompressionBC)
		{
			error = "this GPU can't sample BC compressed textures";
			return false;
		}
		m_Device.WaitIdle();
		ReleaseModelData();

		const auto format = [](TextureFormat value)
		{
			switch (value)
			{
			case TextureFormat::Rgba8Srgb:
				return Rdn::Format::R8G8B8A8_Srgb;
			case TextureFormat::Bc7:
				return Rdn::Format::BC7_Unorm;
			case TextureFormat::Bc7Srgb:
				return Rdn::Format::BC7_Srgb;
			case TextureFormat::Bc5:
				return Rdn::Format::BC5_Unorm;
			default:
				return Rdn::Format::R8G8B8A8_Unorm;
			}
		};
		const auto createTexture = [&](uint32_t width, uint32_t height, uint32_t mips, Rdn::Format imageFormat, const void* pixels, uint64_t size)
		{
			auto image = std::make_unique<Rdn::GpuImage>();
			m_Allocator.CreateGpuImage(image.get(), Rdn::GpuImageDesc{
				.ImageSize = { width, height, 1 },
				.Format = imageFormat,
				.UsageFlags = Rdn::ImageUsage::Sampled | Rdn::ImageUsage::TransferDst | Rdn::ImageUsage::TransferSrc,
				.MipLevels = mips,
			});
			m_Allocator.SetImageData(image.get(), pixels, size, Rdn::ImageLayout::ShaderReadOnlyOptimal);
			return image;
		};
		const uint32_t white = 0xFFFFFFFFu;
		m_Textures.push_back(createTexture(1, 1, 1, Rdn::Format::R8G8B8A8_Unorm, &white, sizeof(white)));
		m_TextureSamplers.push_back(m_NearestSampler.get());
		for (const ModelTexture& texture : data.Textures)
		{
			uint32_t mips = texture.MipCount;
			if (mips == 1 && !texture.Nearest && (texture.Format == TextureFormat::Rgba8 || texture.Format == TextureFormat::Rgba8Srgb))
				while ((std::max(texture.Width, texture.Height) >> mips) > 0)
					mips++;
			m_Textures.push_back(createTexture(texture.Width, texture.Height, mips, format(texture.Format), texture.Pixels.data(), texture.Pixels.size()));
			m_TextureSamplers.push_back(texture.Nearest ? m_NearestSampler.get() : texture.Repeat ? m_RepeatSampler.get() : m_ModelSampler.get());
		}

		const auto slot = [](uint32_t texture) { return texture == NoTexture ? 0u : texture + 1; };
		m_Materials.push_back({});
		for (const ModelMesh& mesh : data.Meshes)
		{
			const bool glows = mesh.Emissive.x + mesh.Emissive.y + mesh.Emissive.z > 0.0f;
			const bool pbr = mesh.Normal != NoTexture || (mesh.Surface != NoTexture && data.Textures[mesh.Surface].Format == TextureFormat::Bc7);
			const bool alphaTested = mesh.AlphaCutoff > 0.0f && !glows;
			MaterialGpu material;
			material.Albedo = glows ? 0 : slot(mesh.Texture);
			material.Surface = glows ? 0 : slot(mesh.Surface);
			material.Normal = glows ? 0 : slot(mesh.Normal);
			material.Flags = (alphaTested ? 1u : 0u) | (pbr ? 2u : 0u);
			material.Color = glm::vec4(glows ? mesh.Emissive : glm::vec3(mesh.Color), alphaTested ? mesh.AlphaCutoff : 0.0f);
			material.Params = glm::vec4(mesh.Roughness, mesh.Metallic, 1.0f, 0.0f);
			m_DrawMeshes.push_back({ .FirstIndex = mesh.FirstIndex, .IndexCount = mesh.IndexCount, .Material = uint32_t(m_Materials.size()), .Skinned = mesh.Skinned, .Glows = glows, .AlphaTested = alphaTested });
			m_Materials.push_back(material);
		}
		m_LibraryMeshes = uint32_t(m_DrawMeshes.size());
		m_TilingMaterialBase = uint32_t(m_Materials.size());
		for (const ModelMaterial& tiling : data.Materials)
		{
			MaterialGpu material;
			material.Albedo = slot(tiling.Albedo);
			material.Surface = slot(tiling.Surface);
			material.Normal = slot(tiling.Normal);
			material.Flags = 2u;
			material.Color = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
			material.Params = glm::vec4(1.0f, 1.0f, 1.0f / tiling.Size, 0.0f);
			m_Materials.push_back(material);
		}
		UploadMaterials();

		for (const ModelEnvironment& source : data.Environments)
		{
			EnvironmentGpu environment;
			environment.Image = createTexture(source.Width, source.Height, source.MipCount, Rdn::Format::R16G16B16A16_Sfloat, source.Pixels.data(), source.Pixels.size() * sizeof(uint16_t));
			for (size_t k = 0; k < 9; k++)
				environment.Irradiance[k] = glm::vec4(source.Irradiance[k], 0.0f);
			environment.MipCount = source.MipCount;
			m_Environments.push_back(std::move(environment));
		}
		UploadGeometry({}, {});
		BindTextures();
		m_ModelDraws.assign(m_DrawMeshes.size(), {});
		m_ModelShadowDraws.assign(m_DrawMeshes.size(), {});
		return true;
	}

	void WorldRenderer::UploadGeometry(const std::vector<ModelVertex>& levelVertices, const std::vector<uint32_t>& levelIndices)
	{
		const ModelLibraryData& data = m_Models.GetData();
		for (Rdn::GpuBuffer* buffer : { m_ModelVertices.get(), m_ModelIndices.get() })
			if (buffer)
				m_Allocator.ReleaseResource(buffer);
		std::vector<ModelVertex> vertices = data.Vertices;
		vertices.insert(vertices.end(), levelVertices.begin(), levelVertices.end());
		std::vector<uint32_t> indices = data.Indices;
		const uint32_t base = uint32_t(data.Vertices.size());
		for (const uint32_t index : levelIndices)
			indices.push_back(base + index);
		m_ModelVertices = std::make_unique<Rdn::GpuBuffer>();
		m_Allocator.CreateGpuBuffer(m_ModelVertices.get(), Rdn::GpuBufferDesc{
			.Size = vertices.size() * sizeof(ModelVertex),
			.UsageFlags = Rdn::BufferUsage::StorageBuffer | Rdn::BufferUsage::TransferDst,
			.MemoryProperty = Rdn::MemoryProperty::DeviceLocal,
		});
		m_Allocator.SetDeviceLocalBufferData(m_ModelVertices.get(), vertices.data(), vertices.size() * sizeof(ModelVertex));
		m_ModelIndices = std::make_unique<Rdn::GpuBuffer>();
		m_Allocator.CreateGpuBuffer(m_ModelIndices.get(), Rdn::GpuBufferDesc{
			.Size = indices.size() * sizeof(uint32_t),
			.UsageFlags = Rdn::BufferUsage::IndexBuffer | Rdn::BufferUsage::TransferDst,
			.MemoryProperty = Rdn::MemoryProperty::DeviceLocal,
		});
		m_Allocator.SetDeviceLocalBufferData(m_ModelIndices.get(), indices.data(), indices.size() * sizeof(uint32_t));
	}

	void WorldRenderer::UploadMaterials()
	{
		if (m_MaterialBuffer)
			m_Allocator.ReleaseResource(m_MaterialBuffer.get());
		m_MaterialBuffer = std::make_unique<Rdn::GpuBuffer>();
		m_Allocator.CreateGpuBuffer(m_MaterialBuffer.get(), Rdn::GpuBufferDesc{
			.Size = m_Materials.size() * sizeof(MaterialGpu),
			.UsageFlags = Rdn::BufferUsage::StorageBuffer | Rdn::BufferUsage::TransferDst,
			.MemoryProperty = Rdn::MemoryProperty::DeviceLocal,
		});
		m_Allocator.SetDeviceLocalBufferData(m_MaterialBuffer.get(), m_Materials.data(), m_Materials.size() * sizeof(MaterialGpu));
	}

	void WorldRenderer::BindTextures()
	{
		Rdn::DescriptorWrite write;
		for (uint32_t i = 0; i < m_Textures.size(); i++)
			write.AddWrite(Rdn::ImageWrite(0, Rdn::DescriptorType::CombinedImageSampler, m_Textures[i]->GetImageView(), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_TextureSamplers[i]->GetHandle(), i));
		const auto bind = [&](std::unique_ptr<Rdn::DescriptorSet>& set, Rdn::Pipeline* pipeline)
		{
			if (!pipeline)
				return;
			set = std::make_unique<Rdn::DescriptorSet>();
			m_Allocator.AllocateDescriptorSet(set.get(), pipeline, 1, uint32_t(m_Textures.size()));
			m_Allocator.UpdateDescriptorSet(set.get(), write);
		};
		bind(m_TextureSet, m_ModelPipeline.get());
		bind(m_ShadowTextureSet, m_ModelAlphaShadowPipeline.get());
		bind(m_BackTextureSet, m_ModelBackPipeline.get());
	}

	uint32_t WorldRenderer::SetLevelGeometry(const std::vector<ModelVertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<LevelMeshDesc>& meshes)
	{
		if (meshes.empty() && m_DrawMeshes.size() == m_LibraryMeshes)
			return m_LibraryMeshes;
		m_Device.WaitIdle();
		m_DrawMeshes.resize(m_LibraryMeshes);
		const uint32_t libraryIndices = uint32_t(m_Models.GetData().Indices.size());
		for (const LevelMeshDesc& desc : meshes)
		{
			const uint32_t material = std::min<uint32_t>(desc.Material, uint32_t(m_Materials.size() - 1));
			m_DrawMeshes.push_back({ .FirstIndex = libraryIndices + desc.FirstIndex, .IndexCount = desc.IndexCount, .Material = material, .Layer = std::min<uint32_t>(desc.Layer, uint32_t(m_Materials.size() - 1)), .Flags = desc.Flags,
				.AlphaTested = (m_Materials[material].Flags & 1u) != 0 });
		}
		UploadGeometry(vertices, indices);
		m_ModelDraws.resize(m_DrawMeshes.size());
		m_ModelShadowDraws.resize(m_DrawMeshes.size());
		return m_LibraryMeshes;
	}

	void WorldRenderer::SetSkyMask(uint32_t width, uint32_t height, const std::vector<uint8_t>& openness, glm::vec2 origin, glm::vec2 size)
	{
		m_Device.WaitIdle();
		if (m_SkyMask)
			m_Allocator.ReleaseResource(m_SkyMask.get());
		const bool valid = width > 0 && height > 0 && openness.size() == size_t(width) * height && size.x > 0.0f && size.y > 0.0f;
		const uint8_t open = 255;
		m_SkyMask = std::make_unique<Rdn::GpuImage>();
		m_Allocator.CreateGpuImage(m_SkyMask.get(), Rdn::GpuImageDesc{ .ImageSize = { valid ? width : 1, valid ? height : 1, 1 }, .Format = Rdn::Format::R8_Unorm, .UsageFlags = Rdn::ImageUsage::Sampled | Rdn::ImageUsage::TransferDst });
		m_Allocator.SetImageData(m_SkyMask.get(), valid ? openness.data() : &open, valid ? openness.size() : 1, Rdn::ImageLayout::ShaderReadOnlyOptimal);
		m_SkyMaskTransform = valid ? glm::vec4(1.0f / size.x, 1.0f / size.y, -origin.x / size.x, -origin.y / size.y) : glm::vec4(0.0f);
	}

	std::optional<uint32_t> WorldRenderer::FindMaterialSlot(std::string_view name) const
	{
		const std::optional<uint32_t> material = m_Models.FindMaterial(name);
		return material ? std::optional<uint32_t>(m_TilingMaterialBase + *material) : std::nullopt;
	}

	std::optional<int32_t> WorldRenderer::FindEnvironment(std::string_view name) const
	{
		const std::optional<uint32_t> environment = m_Models.FindEnvironment(name);
		return environment && *environment < m_Environments.size() ? std::optional<int32_t>(int32_t(*environment)) : std::nullopt;
	}

	EnvironmentView WorldRenderer::GetEnvironmentView() const
	{
		EnvironmentView view;
		view.SkyMask = m_SkyMask.get();
		view.SkyMaskTransform = m_SkyMaskTransform;
		view.Sampler = m_EnvironmentSampler.get();
		view.Image = m_BlackEnvironment.get();
		const int32_t index = m_Lighting.Environment;
		if (index >= 0 && size_t(index) < m_Environments.size())
		{
			const EnvironmentGpu& environment = m_Environments[size_t(index)];
			view.Image = environment.Image.get();
			view.Irradiance = environment.Irradiance;
			view.Params = glm::vec4(m_Lighting.EnvironmentIntensity, std::cos(m_Lighting.EnvironmentRotation), std::sin(m_Lighting.EnvironmentRotation), float(environment.MipCount - 1));
		}
		return view;
	}

	bool WorldRenderer::CreatePipeline()
	{
		Rdn::ShaderDesc vertexDesc;
		Rdn::ShaderDesc fragmentDesc;
		if (!Rdn::ShaderCompiler::Compile("res/Shaders/Game/World.vert", vertexDesc) || !Rdn::ShaderCompiler::Compile("res/Shaders/Game/World.frag", fragmentDesc))
			return false;
		m_VertexShader = std::make_unique<Rdn::Shader>();
		m_FragmentShader = std::make_unique<Rdn::Shader>();
		m_Allocator.CreateShader(m_VertexShader.get(), vertexDesc);
		m_Allocator.CreateShader(m_FragmentShader.get(), fragmentDesc);

		m_Pipeline = std::make_unique<Rdn::Pipeline>();
		m_Allocator.CreatePipeline(m_Pipeline.get(), Rdn::PipelineDesc{
			.ShaderStages = { m_VertexShader.get(), m_FragmentShader.get() },
			.CullMode = Rdn::CullMode::None,
			.ColorAttachmentFormats = { AlbedoFormat, SurfaceFormat, EmissionFormat },
			.DepthAttachmentFormat = DepthFormat,
			.PushDescriptorSets = { 0 },
		});
		return true;
	}

	bool WorldRenderer::CreateModelPipeline()
	{
		Rdn::ShaderDesc vertexDesc;
		Rdn::ShaderDesc fragmentDesc;
		if (!Rdn::ShaderCompiler::Compile("res/Shaders/Game/Model.vert", vertexDesc) || !Rdn::ShaderCompiler::Compile("res/Shaders/Game/Model.frag", fragmentDesc))
			return false;
		m_ModelVertexShader = std::make_unique<Rdn::Shader>();
		m_ModelFragmentShader = std::make_unique<Rdn::Shader>();
		m_Allocator.CreateShader(m_ModelVertexShader.get(), vertexDesc);
		m_Allocator.CreateShader(m_ModelFragmentShader.get(), fragmentDesc);
		m_ModelPipeline = std::make_unique<Rdn::Pipeline>();
		m_Allocator.CreatePipeline(m_ModelPipeline.get(), Rdn::PipelineDesc{
			.ShaderStages = { m_ModelVertexShader.get(), m_ModelFragmentShader.get() },
			.CullMode = Rdn::CullMode::None,
			.ColorAttachmentFormats = { AlbedoFormat, SurfaceFormat, EmissionFormat },
			.DepthAttachmentFormat = DepthFormat,
			.PushDescriptorSets = { 0 },
		});
		if (Rdn::ShaderDesc shadowDesc; Rdn::ShaderCompiler::Compile("res/Shaders/Game/ModelShadow.frag", shadowDesc))
		{
			m_ModelShadowShader = std::make_unique<Rdn::Shader>();
			m_Allocator.CreateShader(m_ModelShadowShader.get(), shadowDesc);
		}
		else
			RDN_LOG_ERROR("The alpha tested shadow shader failed to compile, foliage won't cast shadows");
		if (Rdn::ShaderDesc backDesc; Rdn::ShaderCompiler::Compile("res/Shaders/Game/ModelBack.frag", backDesc))
		{
			m_ModelBackShader = std::make_unique<Rdn::Shader>();
			m_Allocator.CreateShader(m_ModelBackShader.get(), backDesc);
		}
		else
			RDN_LOG_ERROR("The back face shader failed to compile, indirect light uses a fixed object thickness");
		return true;
	}

	void WorldRenderer::CreateShadowPipelines()
	{
		const auto create = [&](std::initializer_list<Rdn::Shader*> stages)
		{
			auto pipeline = std::make_unique<Rdn::Pipeline>();
			m_Allocator.CreatePipeline(pipeline.get(), Rdn::PipelineDesc{
				.ShaderStages = stages,
				.CullMode = Rdn::CullMode::None,
				.DepthAttachmentFormat = ShadowFormat,
				.PushDescriptorSets = { 0 },
			});
			return pipeline;
		};
		if (m_Pipeline)
			m_ShadowPipeline = create({ m_VertexShader.get() });
		if (m_ModelPipeline)
			m_ModelShadowPipeline = create({ m_ModelVertexShader.get() });
		if (m_ModelPipeline && m_ModelShadowShader)
			m_ModelAlphaShadowPipeline = create({ m_ModelVertexShader.get(), m_ModelShadowShader.get() });
		const auto createBack = [&](std::initializer_list<Rdn::Shader*> stages, Rdn::CullMode cull)
		{
			auto pipeline = std::make_unique<Rdn::Pipeline>();
			m_Allocator.CreatePipeline(pipeline.get(), Rdn::PipelineDesc{
				.ShaderStages = stages,
				.CullMode = cull,
				.DepthAttachmentFormat = DepthFormat,
				.PushDescriptorSets = { 0 },
			});
			return pipeline;
		};
		if (m_Pipeline)
			m_BackPipeline = createBack({ m_VertexShader.get() }, Rdn::CullMode::Front);
		if (m_ModelPipeline && m_ModelBackShader)
			m_ModelBackPipeline = createBack({ m_ModelVertexShader.get(), m_ModelBackShader.get() }, Rdn::CullMode::None);
	}

	void WorldRenderer::CreateMeshes()
	{
		Mesh meshes[] = { MakeBox(), MakeCylinder(), MakeCylinderX(), MakeCone(), MakeSphere(), MakeWedge(), MakeRoof() };
		static_assert(std::size(meshes) == size_t(MeshKind::Count));
		for (Mesh& mesh : meshes)
			for (size_t i = 0; i + 2 < mesh.size(); i += 3)
			{
				const glm::vec3 a(mesh[i].Position);
				const glm::vec3 normal = glm::vec3(mesh[i].Normal) + glm::vec3(mesh[i + 1].Normal) + glm::vec3(mesh[i + 2].Normal);
				if (glm::dot(glm::cross(glm::vec3(mesh[i + 1].Position) - a, glm::vec3(mesh[i + 2].Position) - a), normal) < 0.0f)
					std::swap(mesh[i + 1], mesh[i + 2]);
			}
		std::vector<MeshVertex> vertices;
		for (size_t i = 0; i < std::size(meshes); i++)
		{
			m_Meshes[i] = { uint32_t(vertices.size()), uint32_t(meshes[i].size()) };
			vertices.insert(vertices.end(), meshes[i].begin(), meshes[i].end());
		}
		m_MeshBuffer = std::make_unique<Rdn::GpuBuffer>();
		m_Allocator.CreateGpuBuffer(m_MeshBuffer.get(), Rdn::GpuBufferDesc{
			.Size = vertices.size() * sizeof(MeshVertex),
			.UsageFlags = Rdn::BufferUsage::StorageBuffer,
			.MemoryProperty = Rdn::MemoryProperty::HostVisible | Rdn::MemoryProperty::HostCoherent,
			.Mapped = true,
		});
		m_MeshBuffer->Write(vertices.data(), vertices.size() * sizeof(MeshVertex));
	}

	void WorldRenderer::EnsureCapacity(uint32_t instances)
	{
		if (instances <= m_Capacity)
			return;
		if (m_InstanceBuffer)
		{
			m_Device.WaitIdle();
			m_Allocator.ReleaseResource(m_InstanceBuffer.get());
		}
		m_Capacity = std::max(instances + instances / 2, 16384u);
		m_Capacity = (m_Capacity + 15) / 16 * 16;
		m_InstanceBuffer = std::make_unique<Rdn::GpuRingBuffer>();
		m_Allocator.CreateGpuRingBuffer(m_InstanceBuffer.get(), Rdn::GpuRingBufferDesc{
			.ElementSize = uint32_t(m_Capacity * sizeof(BoxInstance)),
			.ElementCount = m_Device.GetFramesInFlightCount(),
			.UsageFlags = Rdn::BufferUsage::StorageBuffer,
			.MemoryProperty = Rdn::MemoryProperty::HostVisible | Rdn::MemoryProperty::HostCoherent,
			.Mapped = true,
		});
	}

	void WorldRenderer::EnsureModelCapacity(uint32_t instances, uint32_t joints)
	{
		const auto grow = [&](std::unique_ptr<Rdn::GpuRingBuffer>& buffer, uint32_t& capacity, uint32_t needed, uint32_t minimum, size_t stride)
		{
			if (needed <= capacity)
				return;
			if (buffer)
			{
				m_Device.WaitIdle();
				m_Allocator.ReleaseResource(buffer.get());
			}
			capacity = std::max((needed + needed / 2 + 255) / 256 * 256, minimum);
			buffer = std::make_unique<Rdn::GpuRingBuffer>();
			m_Allocator.CreateGpuRingBuffer(buffer.get(), Rdn::GpuRingBufferDesc{
				.ElementSize = uint32_t(capacity * stride),
				.ElementCount = m_Device.GetFramesInFlightCount(),
				.UsageFlags = Rdn::BufferUsage::StorageBuffer,
				.MemoryProperty = Rdn::MemoryProperty::HostVisible | Rdn::MemoryProperty::HostCoherent,
				.Mapped = true,
			});
		};
		grow(m_ModelInstanceBuffer, m_ModelCapacity, instances, 1024, sizeof(ModelInstance));
		grow(m_PaletteBuffer, m_PaletteCapacity, joints, 4096, sizeof(glm::mat4));
	}

	void WorldRenderer::Begin(const WorldCamera& camera, const SceneLighting& lighting, float focusHeight, bool fadeBelowFocus)
	{
		for (std::vector<BoxInstance>& instances : m_Instances)
			instances.clear();
		for (std::vector<BoxInstance>& casters : m_ShadowCasters)
			casters.clear();
		for (std::vector<ModelInstance>& draws : m_ModelDraws)
			draws.clear();
		for (std::vector<ModelInstance>& draws : m_ModelShadowDraws)
			draws.clear();
		m_Palettes.clear();
		m_Particles->BeginFrame();
		m_Camera = camera;
		m_Lighting = lighting;
		m_FocusHeight = focusHeight;
		m_Constants.ViewProjection = camera.ViewProjection;
		m_Constants.Focus = glm::vec4(fadeBelowFocus ? focusHeight : -1e4f, 0.0f, 0.0f, 0.0f);
	}

	void WorldRenderer::DrawShadowCaster(MeshKind mesh, const glm::vec3& center, const glm::vec3& halfExtents, float yaw)
	{
		if (size_t(mesh) < m_ShadowCasters.size())
			m_ShadowCasters[size_t(mesh)].push_back({ glm::vec4(center, yaw), glm::vec4(halfExtents, 0.0f), glm::vec4(0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f) });
	}

	void WorldRenderer::Draw(MeshKind mesh, const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec3& color, float yaw, float emission, float ground, const BoxSurface& surface)
	{
		if (size_t(mesh) < m_Instances.size())
			m_Instances[size_t(mesh)].push_back({ glm::vec4(center, yaw), glm::vec4(halfExtents, ground), glm::vec4(color, emission), glm::vec4(surface.Roughness, surface.Metallic, 0.0f, 0.0f) });
	}

	uint32_t WorldRenderer::AddPalette(const JointMatrices& skin)
	{
		const uint32_t offset = uint32_t(m_Palettes.size());
		const uint32_t joints = std::min(m_Models.GetSkeleton().JointCount(), MaxModelJoints);
		m_Palettes.insert(m_Palettes.end(), skin.begin(), skin.begin() + joints);
		return offset;
	}

	void WorldRenderer::DrawModel(uint32_t mesh, const glm::mat4& transform, uint32_t palette, const glm::vec4& tint, float emission, float ground)
	{
		if (mesh < m_ModelDraws.size())
			m_ModelDraws[mesh].push_back(MakeModelInstance(mesh, transform, palette, tint, emission, ground));
	}

	ModelInstance WorldRenderer::MakeModelInstance(uint32_t mesh, const glm::mat4& transform, uint32_t palette, const glm::vec4& tint, float emission, float ground) const
	{
		const DrawMesh& source = m_DrawMeshes[mesh < m_DrawMeshes.size() ? mesh : 0];
		const glm::mat4 rows = glm::transpose(transform);
		return {
			.Rows = { rows[0], rows[1], rows[2] },
			.Tint = tint,
			.Shading = glm::vec4(source.Glows ? 1.0f : emission, ground, 0.0f, 0.0f),
			.Info = glm::uvec4(source.Skinned ? palette : NoPalette, source.Material, source.Layer, source.Flags),
		};
	}

	void WorldRenderer::SubmitModel(uint32_t mesh, const ModelInstance& instance, bool shadowOnly)
	{
		if (mesh < m_ModelDraws.size())
			(shadowOnly ? m_ModelShadowDraws : m_ModelDraws)[mesh].push_back(instance);
	}

	void WorldRenderer::Render(Rdn::RenderGraph& graph, Rdn::RGImage target, Rdn::Extent2D output, uint32_t frameIndex, const std::array<float, 4>& clearColor)
	{
		const bool gi = HasGi();
		const float guardBand = GetGuardBand();
		const glm::uvec2 guard(uint32_t(std::lround(float(output.Width) * guardBand)), uint32_t(std::lround(float(output.Height) * guardBand)));
		const Rdn::Extent2D extent{ output.Width + 2 * guard.x, output.Height + 2 * guard.y };
		WorldCamera camera = m_Camera;
		camera.Projection[0][0] *= float(output.Width) / float(extent.Width);
		camera.Projection[1][1] *= float(output.Height) / float(extent.Height);
		camera.ViewProjection = camera.Projection * camera.View;
		m_Constants.ViewProjection = camera.ViewProjection;
		const glm::ivec2 anchor = m_Gi ? m_Gi->Update(extent, camera, m_FocusHeight) : glm::ivec2(0);
		m_Constants.Focus.z = float(((anchor.x % 4) + 4) % 4);
		m_Constants.Focus.w = float(((anchor.y % 4) + 4) % 4);

		m_Packed.clear();
		for (size_t i = 0; i < m_BoxRanges.size(); i++)
		{
			const std::vector<BoxInstance>& source = i < m_Instances.size() ? m_Instances[i] : m_ShadowCasters[i - m_Instances.size()];
			m_BoxRanges[i] = { uint32_t(m_Packed.size()), uint32_t(source.size()) };
			m_Packed.insert(m_Packed.end(), source.begin(), source.end());
		}
		const uint32_t count = m_Pipeline ? uint32_t(m_Packed.size()) : 0;
		if (count > 0)
		{
			EnsureCapacity(count);
			std::memcpy(static_cast<uint8_t*>(m_InstanceBuffer->GetMappedPtr()) + m_InstanceBuffer->GetOffset(frameIndex), m_Packed.data(), count * sizeof(BoxInstance));
		}

		m_ModelRanges.resize(m_ModelDraws.size());
		m_ModelShadowRanges.resize(m_ModelDraws.size());
		m_PackedModels.clear();
		for (size_t mesh = 0; mesh < m_ModelDraws.size(); mesh++)
		{
			m_ModelRanges[mesh] = { uint32_t(m_PackedModels.size()), uint32_t(m_ModelDraws[mesh].size()) };
			m_PackedModels.insert(m_PackedModels.end(), m_ModelDraws[mesh].begin(), m_ModelDraws[mesh].end());
		}
		for (size_t mesh = 0; mesh < m_ModelShadowDraws.size(); mesh++)
		{
			m_ModelShadowRanges[mesh] = { uint32_t(m_PackedModels.size()), uint32_t(m_ModelShadowDraws[mesh].size()) };
			m_PackedModels.insert(m_PackedModels.end(), m_ModelShadowDraws[mesh].begin(), m_ModelShadowDraws[mesh].end());
		}
		const uint32_t modelCount = m_ModelPipeline && m_ModelIndices ? uint32_t(m_PackedModels.size()) : 0;
		if (m_Palettes.empty())
			m_Palettes.push_back(glm::mat4(1.0f));
		const uint32_t paletteCount = uint32_t(m_Palettes.size());
		if (modelCount > 0)
		{
			EnsureModelCapacity(modelCount, paletteCount);
			std::memcpy(static_cast<uint8_t*>(m_ModelInstanceBuffer->GetMappedPtr()) + m_ModelInstanceBuffer->GetOffset(frameIndex), m_PackedModels.data(), modelCount * sizeof(ModelInstance));
			std::memcpy(static_cast<uint8_t*>(m_PaletteBuffer->GetMappedPtr()) + m_PaletteBuffer->GetOffset(frameIndex), m_Palettes.data(), paletteCount * sizeof(glm::mat4));
		}
		m_LastCount = count + modelCount;

		const auto image = [&](const char* name, Rdn::Format format) { return graph.CreateImage(name, Rdn::RGImageDesc{ .Size = Rdn::Extent3D(extent), .Format = format }); };
		const GBuffer gbuffer{
			.Albedo = image("GBuffer albedo", AlbedoFormat),
			.Surface = image("GBuffer surface", SurfaceFormat),
			.Emission = image("Scene emission", EmissionFormat),
			.Depth = image("World depth", DepthFormat),
		};
		const Rdn::RGImage scene = image("Scene color", SceneColorFormat);
		const glm::vec3 background = glm::pow(glm::vec3(clearColor[0], clearColor[1], clearColor[2]), glm::vec3(2.2f));
		graph.AddPass("GBuffer", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				for (const Rdn::RGImage attachment : { gbuffer.Albedo, gbuffer.Surface, gbuffer.Emission })
					builder.AddColorAttachment(attachment, Rdn::AttachmentLoadOp::Clear, Rdn::AttachmentStoreOp::Store, { 0.0f, 0.0f, 0.0f, 0.0f });
				builder.SetDepthAttachment(gbuffer.Depth, Rdn::AttachmentLoadOp::Clear, Rdn::AttachmentStoreOp::Store);
			},
			[this, count, modelCount, paletteCount, frameIndex](Rdn::CommandBuffer& cmd, const Rdn::PassContext&)
			{
				if (count > 0)
				{
					cmd.BindPipeline(m_Pipeline->GetHandle());
					cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Pipeline->GetLayout(), 0, Rdn::DescriptorWrite()
						.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_InstanceBuffer->GetHandle(), m_InstanceBuffer->GetOffset(frameIndex), uint64_t(count) * sizeof(BoxInstance)))
						.AddWrite(Rdn::BufferWrite(1, Rdn::DescriptorType::StorageBuffer, m_MeshBuffer->GetHandle(), 0, m_MeshBuffer->GetSize())));
					cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_Pipeline->GetLayout(), &m_Constants, sizeof(m_Constants));
					for (size_t mesh = 0; mesh < m_Instances.size(); mesh++)
						if (m_BoxRanges[mesh].second > 0)
							cmd.Draw(m_Meshes[mesh].VertexCount, m_BoxRanges[mesh].second, m_Meshes[mesh].FirstVertex, m_BoxRanges[mesh].first);
				}
				if (modelCount == 0 || !m_TextureSet)
					return;
				Rdn::DescriptorWrite write;
				write.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_ModelInstanceBuffer->GetHandle(), m_ModelInstanceBuffer->GetOffset(frameIndex), uint64_t(modelCount) * sizeof(ModelInstance)));
				write.AddWrite(Rdn::BufferWrite(1, Rdn::DescriptorType::StorageBuffer, m_ModelVertices->GetHandle(), 0, m_ModelVertices->GetSize()));
				write.AddWrite(Rdn::BufferWrite(2, Rdn::DescriptorType::StorageBuffer, m_PaletteBuffer->GetHandle(), m_PaletteBuffer->GetOffset(frameIndex), uint64_t(paletteCount) * sizeof(glm::mat4)));
				write.AddWrite(Rdn::BufferWrite(3, Rdn::DescriptorType::StorageBuffer, m_MaterialBuffer->GetHandle(), 0, m_MaterialBuffer->GetSize()));
				cmd.BindPipeline(m_ModelPipeline->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_ModelPipeline->GetLayout(), 0, write);
				cmd.BindDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_ModelPipeline->GetLayout(), 1, { m_TextureSet->GetHandle() });
				cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_ModelPipeline->GetLayout(), &m_Constants, sizeof(m_Constants));
				cmd.BindIndexBuffer(m_ModelIndices->GetHandle(), Rdn::IndexType::Uint32);
				for (size_t mesh = 0; mesh < m_ModelRanges.size(); mesh++)
					if (m_ModelRanges[mesh].second > 0)
					{
						const DrawMesh& source = m_DrawMeshes[mesh];
						cmd.DrawIndexed(source.IndexCount, m_ModelRanges[mesh].second, source.FirstIndex, 0, m_ModelRanges[mesh].first);
					}
			});
		Rdn::RGImage backDepth{};
		if (gi && GiPresets[size_t(m_GiQuality)].BackFaces && m_BackPipeline)
		{
			backDepth = image("World back depth", DepthFormat);
			RenderBackFaces(graph, backDepth, count, modelCount, paletteCount, frameIndex);
		}
		const uint32_t resolution = ShadowResolution[size_t(m_ShadowQuality)];
		const ShadowView shadow = FitShadowView(camera, m_Lighting.SunDirection, m_FocusHeight, resolution, m_ShadowExtent);
		const Rdn::RGImage shadowMap = graph.CreateImage("Shadow map", Rdn::RGImageDesc{ .Size = { resolution, resolution, 1 }, .Format = ShadowFormat });
		RenderShadows(graph, shadowMap, shadow, count, modelCount, paletteCount, frameIndex);
		m_Particles->Upload(extent, frameIndex, camera);
		m_Particles->RenderDecals(graph, gbuffer, frameIndex);
		m_Particles->Simulate(graph, gbuffer.Depth, frameIndex);
		const Rdn::Extent2D half = HalfExtent(extent);
		Rdn::RGImage giSource{};
		Rdn::RGImage emitters{};
		if (gi)
		{
			giSource = m_Gi->CreateSource(graph, extent);
			emitters = graph.CreateImage("Particle light", Rdn::RGImageDesc{ .Size = { half.Width, half.Height, 1 }, .Format = ParticleLightFormat });
			m_Particles->DrawLight(graph, emitters, gbuffer.Depth, frameIndex);
		}
		const float edgeBand = guard.x > 0 ? 0.5f * float(std::min(guard.x, guard.y)) : 0.04f * float(std::min(half.Width, half.Height));
		const EnvironmentView environment = GetEnvironmentView();
		m_Deferred->Render(graph, gbuffer, scene, shadowMap, giSource, emitters, extent, frameIndex, camera, m_Lighting, environment, shadow, m_ShadowQuality, m_Constants.Focus.x, edgeBand, background, m_View);
		if (gi)
			m_Gi->Render(graph, gbuffer, giSource, emitters, backDepth, scene, extent, frameIndex, camera, m_Lighting, environment, m_GiQuality, m_Constants.Focus.x, edgeBand, m_View);
		if (m_View != DebugView::Irradiance && m_View != DebugView::GiSource)
			m_Particles->Draw(graph, scene, gbuffer.Emission, gbuffer.Depth, frameIndex);
		m_Post->Raw = IsRawView(m_View);
		m_Post->SceneExposure = m_Lighting.Exposure;
		m_Post->Render(graph, scene, gbuffer.Emission, target, extent, output);
	}

	void WorldRenderer::RenderBackFaces(Rdn::RenderGraph& graph, Rdn::RGImage target, uint32_t boxCount, uint32_t modelCount, uint32_t paletteCount, uint32_t frameIndex)
	{
		graph.AddPass("GI back faces", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder) { builder.SetDepthAttachment(target, Rdn::AttachmentLoadOp::Clear, Rdn::AttachmentStoreOp::Store); },
			[this, boxCount, modelCount, paletteCount, frameIndex](Rdn::CommandBuffer& cmd, const Rdn::PassContext&)
			{
				if (boxCount > 0 && m_BackPipeline)
				{
					cmd.BindPipeline(m_BackPipeline->GetHandle());
					cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_BackPipeline->GetLayout(), 0, Rdn::DescriptorWrite()
						.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_InstanceBuffer->GetHandle(), m_InstanceBuffer->GetOffset(frameIndex), uint64_t(boxCount) * sizeof(BoxInstance)))
						.AddWrite(Rdn::BufferWrite(1, Rdn::DescriptorType::StorageBuffer, m_MeshBuffer->GetHandle(), 0, m_MeshBuffer->GetSize())));
					cmd.PushConstants(Rdn::ShaderStage::Vertex, m_BackPipeline->GetLayout(), &m_Constants, sizeof(m_Constants));
					for (size_t mesh = 0; mesh < m_Instances.size(); mesh++)
						if (m_BoxRanges[mesh].second > 0)
							cmd.Draw(m_Meshes[mesh].VertexCount, m_BoxRanges[mesh].second, m_Meshes[mesh].FirstVertex, m_BoxRanges[mesh].first);
				}
				if (modelCount == 0 || !m_ModelBackPipeline || !m_BackTextureSet)
					return;
				Rdn::DescriptorWrite write;
				write.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_ModelInstanceBuffer->GetHandle(), m_ModelInstanceBuffer->GetOffset(frameIndex), uint64_t(modelCount) * sizeof(ModelInstance)));
				write.AddWrite(Rdn::BufferWrite(1, Rdn::DescriptorType::StorageBuffer, m_ModelVertices->GetHandle(), 0, m_ModelVertices->GetSize()));
				write.AddWrite(Rdn::BufferWrite(2, Rdn::DescriptorType::StorageBuffer, m_PaletteBuffer->GetHandle(), m_PaletteBuffer->GetOffset(frameIndex), uint64_t(paletteCount) * sizeof(glm::mat4)));
				write.AddWrite(Rdn::BufferWrite(3, Rdn::DescriptorType::StorageBuffer, m_MaterialBuffer->GetHandle(), 0, m_MaterialBuffer->GetSize()));
				cmd.BindPipeline(m_ModelBackPipeline->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_ModelBackPipeline->GetLayout(), 0, write);
				cmd.BindDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_ModelBackPipeline->GetLayout(), 1, { m_BackTextureSet->GetHandle() });
				cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_ModelBackPipeline->GetLayout(), &m_Constants, sizeof(m_Constants));
				cmd.BindIndexBuffer(m_ModelIndices->GetHandle(), Rdn::IndexType::Uint32);
				for (size_t mesh = 0; mesh < m_ModelRanges.size(); mesh++)
					if (m_ModelRanges[mesh].second > 0)
					{
						const DrawMesh& source = m_DrawMeshes[mesh];
						cmd.DrawIndexed(source.IndexCount, m_ModelRanges[mesh].second, source.FirstIndex, 0, m_ModelRanges[mesh].first);
					}
			});
	}

	void WorldRenderer::RenderShadows(Rdn::RenderGraph& graph, Rdn::RGImage shadowMap, const ShadowView& shadow, uint32_t boxCount, uint32_t modelCount, uint32_t paletteCount, uint32_t frameIndex)
	{
		const PushConstants constants{ shadow.ViewProjection, glm::vec4(m_Constants.Focus.x, 1.0f, 0.0f, 0.0f) };
		const bool draw = m_ShadowQuality != ShadowQuality::Off;
		graph.AddPass("Shadows", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder) { builder.SetDepthAttachment(shadowMap, Rdn::AttachmentLoadOp::Clear, Rdn::AttachmentStoreOp::Store); },
			[this, constants, draw, boxCount, modelCount, paletteCount, frameIndex](Rdn::CommandBuffer& cmd, const Rdn::PassContext&)
			{
				if (!draw)
					return;
				if (boxCount > 0 && m_ShadowPipeline)
				{
					cmd.BindPipeline(m_ShadowPipeline->GetHandle());
					cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_ShadowPipeline->GetLayout(), 0, Rdn::DescriptorWrite()
						.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_InstanceBuffer->GetHandle(), m_InstanceBuffer->GetOffset(frameIndex), uint64_t(boxCount) * sizeof(BoxInstance)))
						.AddWrite(Rdn::BufferWrite(1, Rdn::DescriptorType::StorageBuffer, m_MeshBuffer->GetHandle(), 0, m_MeshBuffer->GetSize())));
					cmd.PushConstants(Rdn::ShaderStage::Vertex, m_ShadowPipeline->GetLayout(), &constants, sizeof(constants));
					for (size_t range = 0; range < m_BoxRanges.size(); range++)
						if (m_BoxRanges[range].second > 0)
						{
							const MeshRange& mesh = m_Meshes[range % m_Meshes.size()];
							cmd.Draw(mesh.VertexCount, m_BoxRanges[range].second, mesh.FirstVertex, m_BoxRanges[range].first);
						}
				}
				if (modelCount == 0 || !m_ModelShadowPipeline)
					return;
				Rdn::DescriptorWrite write;
				write.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_ModelInstanceBuffer->GetHandle(), m_ModelInstanceBuffer->GetOffset(frameIndex), uint64_t(modelCount) * sizeof(ModelInstance)));
				write.AddWrite(Rdn::BufferWrite(1, Rdn::DescriptorType::StorageBuffer, m_ModelVertices->GetHandle(), 0, m_ModelVertices->GetSize()));
				write.AddWrite(Rdn::BufferWrite(2, Rdn::DescriptorType::StorageBuffer, m_PaletteBuffer->GetHandle(), m_PaletteBuffer->GetOffset(frameIndex), uint64_t(paletteCount) * sizeof(glm::mat4)));
				cmd.BindPipeline(m_ModelShadowPipeline->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_ModelShadowPipeline->GetLayout(), 0, write);
				cmd.PushConstants(Rdn::ShaderStage::Vertex, m_ModelShadowPipeline->GetLayout(), &constants, sizeof(constants));
				cmd.BindIndexBuffer(m_ModelIndices->GetHandle(), Rdn::IndexType::Uint32);
				bool alphaTested = false;
				const auto draw = [&](bool alpha)
				{
					for (const auto* ranges : { &m_ModelRanges, &m_ModelShadowRanges })
						for (size_t mesh = 0; mesh < ranges->size(); mesh++)
						{
							const auto [first, instances] = (*ranges)[mesh];
							const DrawMesh& source = m_DrawMeshes[mesh];
							alphaTested |= instances > 0 && source.AlphaTested;
							if (instances > 0 && source.AlphaTested == alpha)
								cmd.DrawIndexed(source.IndexCount, instances, source.FirstIndex, 0, first);
						}
				};
				draw(false);
				if (!alphaTested || !m_ModelAlphaShadowPipeline || !m_ShadowTextureSet)
					return;
				write.AddWrite(Rdn::BufferWrite(3, Rdn::DescriptorType::StorageBuffer, m_MaterialBuffer->GetHandle(), 0, m_MaterialBuffer->GetSize()));
				cmd.BindPipeline(m_ModelAlphaShadowPipeline->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_ModelAlphaShadowPipeline->GetLayout(), 0, write);
				cmd.BindDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_ModelAlphaShadowPipeline->GetLayout(), 1, { m_ShadowTextureSet->GetHandle() });
				cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_ModelAlphaShadowPipeline->GetLayout(), &constants, sizeof(constants));
				draw(true);
			});
	}
}

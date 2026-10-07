#include "WorldRenderer.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Graphics/ShaderCompiler.h>
#include <algorithm>
#include <cmath>
#include <cstring>
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
		if (!CreatePipeline(colorFormat))
			RDN_LOG_ERROR("The world shaders failed to compile, the world won't be drawn");
		CreateMeshes();
		EnsureCapacity(16384);
	}

	WorldRenderer::~WorldRenderer()
	{
		m_Device.WaitIdle();
		if (m_InstanceBuffer)
			m_Allocator.ReleaseResource(m_InstanceBuffer.get());
		if (m_MeshBuffer)
			m_Allocator.ReleaseResource(m_MeshBuffer.get());
		if (m_Pipeline)
		{
			m_Allocator.ReleaseResource(m_Pipeline.get());
			m_Allocator.ReleaseResource(m_VertexShader.get());
			m_Allocator.ReleaseResource(m_FragmentShader.get());
		}
	}

	bool WorldRenderer::CreatePipeline(Rdn::Format colorFormat)
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
			.ColorAttachmentFormats = { colorFormat },
			.DepthAttachmentFormat = Rdn::Format::D32_Sfloat,
			.PushDescriptorSets = { 0 },
		});
		return true;
	}

	void WorldRenderer::CreateMeshes()
	{
		const Mesh meshes[] = { MakeBox(), MakeCylinder(), MakeCylinderX(), MakeCone(), MakeSphere(), MakeWedge(), MakeRoof() };
		static_assert(std::size(meshes) == size_t(MeshKind::Count));
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

	void WorldRenderer::Begin(const glm::mat4& viewProjection, const glm::vec3& lightDirection, float focusHeight)
	{
		for (std::vector<BoxInstance>& instances : m_Instances)
			instances.clear();
		m_Constants.ViewProjection = viewProjection;
		m_Constants.LightDirection = glm::vec4(glm::normalize(lightDirection), focusHeight);
	}

	void WorldRenderer::Draw(MeshKind mesh, const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec3& color, float yaw, float emission, float ground)
	{
		if (size_t(mesh) < m_Instances.size())
			m_Instances[size_t(mesh)].push_back({ glm::vec4(center, yaw), glm::vec4(halfExtents, ground), glm::vec4(color, emission) });
	}

	void WorldRenderer::Render(Rdn::RenderGraph& graph, Rdn::RGImage target, Rdn::Extent2D extent, uint32_t frameIndex, const std::array<float, 4>& clearColor)
	{
		std::array<std::pair<uint32_t, uint32_t>, size_t(MeshKind::Count)> ranges{};
		m_Packed.clear();
		for (size_t i = 0; i < m_Instances.size(); i++)
		{
			ranges[i] = { uint32_t(m_Packed.size()), uint32_t(m_Instances[i].size()) };
			m_Packed.insert(m_Packed.end(), m_Instances[i].begin(), m_Instances[i].end());
		}
		const uint32_t count = m_Pipeline ? uint32_t(m_Packed.size()) : 0;
		m_LastCount = count;
		if (count > 0)
		{
			EnsureCapacity(count);
			std::memcpy(static_cast<uint8_t*>(m_InstanceBuffer->GetMappedPtr()) + m_InstanceBuffer->GetOffset(frameIndex), m_Packed.data(), count * sizeof(BoxInstance));
		}

		const Rdn::RGImage depth = graph.CreateImage("World depth", Rdn::RGImageDesc{ .Size = Rdn::Extent3D(extent), .Format = Rdn::Format::D32_Sfloat });
		graph.AddPass("World", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(target, Rdn::AttachmentLoadOp::Clear, Rdn::AttachmentStoreOp::Store, clearColor);
				builder.SetDepthAttachment(depth);
			},
			[this, count, ranges, frameIndex](Rdn::CommandBuffer& cmd, const Rdn::PassContext&)
			{
				if (count == 0)
					return;
				cmd.BindPipeline(m_Pipeline->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Pipeline->GetLayout(), 0, Rdn::DescriptorWrite()
					.AddWrite(Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_InstanceBuffer->GetHandle(), m_InstanceBuffer->GetOffset(frameIndex), uint64_t(count) * sizeof(BoxInstance)))
					.AddWrite(Rdn::BufferWrite(1, Rdn::DescriptorType::StorageBuffer, m_MeshBuffer->GetHandle(), 0, m_MeshBuffer->GetSize())));
				cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_Pipeline->GetLayout(), &m_Constants, sizeof(m_Constants));
				for (size_t mesh = 0; mesh < ranges.size(); mesh++)
					if (ranges[mesh].second > 0)
						cmd.Draw(m_Meshes[mesh].VertexCount, ranges[mesh].second, m_Meshes[mesh].FirstVertex, ranges[mesh].first);
			});
	}
}

#include "MenuBackdrop.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Graphics/ShaderCompiler.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <vector>

namespace Game
{
	namespace
	{
		constexpr float FlightSeconds = 1.8f;
		constexpr float FieldOfView = 0.82f;
		constexpr float WardenScale = 5.4f;
		constexpr float WardenDepth = 1.62f;
		constexpr glm::vec3 HandCentre{ 0.125f, -0.05f, 0.3f };
		constexpr glm::vec2 HandReach{ 0.75f, 0.65f };
		constexpr float HandTilt = 0.35f;
		constexpr float HandFacing = 0.08f;
		constexpr float HandScale = 1.0f;
		constexpr glm::vec2 HandCurl{ -0.6f, -0.25f };

		struct PassConstants
		{
			uint32_t Kind = 0;
			uint32_t Count = 0;
			uint32_t Band = 0;
			uint32_t Bands = 1;
		};

		float Smoother(float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
		}

		glm::vec3 WardenTurn(glm::vec3 v, float yaw, float lean, float roll)
		{
			v = glm::vec3(v.x * std::cos(roll) - v.y * std::sin(roll), v.x * std::sin(roll) + v.y * std::cos(roll), v.z);
			v = glm::vec3(v.x, v.y * std::cos(lean) + v.z * std::sin(lean), -v.y * std::sin(lean) + v.z * std::cos(lean));
			return glm::vec3(v.x * std::cos(yaw) + v.z * std::sin(yaw), v.y, -v.x * std::sin(yaw) + v.z * std::cos(yaw));
		}
	}

	MenuBackdrop::MenuBackdrop(Rdn::Device& device, Rdn::ResourceAllocator& allocator, Rdn::Format targetFormat, const Rdn::GpuImage* atlas)
		: m_Device(device), m_Allocator(allocator), m_Format(targetFormat), m_Atlas(atlas)
	{
		if (device.GetProperties().Integrated)
		{
			m_Scale = 0.75f;
			m_Quality = 0.0f;
			m_NebulaSize = 1024;
			m_BakeBands = 32;
			m_GalaxyStars = 60000;
			m_DustMotes = 1400;
			m_TitleMotes = 180;
			m_WardenStars = 60000;
		}
		m_Bake = CreatePipeline("res/Shaders/Game/Composite.vert", "res/Shaders/Game/CosmosBake.frag", HdrFormat, Rdn::BlendMode::Opaque);
		m_Sky = CreatePipeline("res/Shaders/Game/Composite.vert", "res/Shaders/Game/Cosmos.frag", HdrFormat, Rdn::BlendMode::Opaque);
		m_Points = CreatePipeline("res/Shaders/Game/CosmosPoints.vert", "res/Shaders/Game/CosmosPoints.frag", HdrFormat, Rdn::BlendMode::Additive);
		m_Composite = CreatePipeline("res/Shaders/Game/Composite.vert", "res/Shaders/Game/CosmosComposite.frag", targetFormat, Rdn::BlendMode::Opaque);
		m_LightPass = CreatePipeline("res/Shaders/Game/Composite.vert", "res/Shaders/Game/CosmosLight.frag", LightFormat, Rdn::BlendMode::Opaque);
		m_Bloom = CreatePipeline("res/Shaders/Game/Composite.vert", "res/Shaders/Game/CosmosBloom.frag", HdrFormat, Rdn::BlendMode::Opaque);
		m_Veil = CreatePipeline("res/Shaders/Game/Composite.vert", "res/Shaders/Game/CosmosVeil.frag", HdrFormat, Rdn::BlendMode::Opaque);
		if (!m_Bake || !m_Sky || !m_Points || !m_Composite || !m_LightPass || !m_Bloom || !m_Veil || !m_Atlas)
		{
			RDN_LOG_ERROR("The cosmos shaders failed to compile, the menu falls back to a flat background");
			return;
		}
		m_Nebula = std::make_unique<Rdn::GpuImage>();
		m_Allocator.CreateGpuImage(m_Nebula.get(), Rdn::GpuImageDesc{ .ImageSize = { m_NebulaSize, m_NebulaSize, 1 }, .Format = HdrFormat, .UsageFlags = Rdn::ImageUsage::Sampled | Rdn::ImageUsage::ColorAttachment });
		m_Sampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_Sampler.get(), Rdn::SamplerDesc{ .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
		m_ClampSampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_ClampSampler.get(), Rdn::SamplerDesc{ .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
		m_Buffer = std::make_unique<Rdn::GpuRingBuffer>();
		m_Allocator.CreateGpuRingBuffer(m_Buffer.get(), Rdn::GpuRingBufferDesc{
			.ElementSize = uint32_t(sizeof(Frame) + MaxGlyphs * sizeof(Glyph)),
			.ElementCount = m_Device.GetFramesInFlightCount(),
			.UsageFlags = Rdn::BufferUsage::StorageBuffer,
			.MemoryProperty = Rdn::MemoryProperty::HostVisible | Rdn::MemoryProperty::HostCoherent,
			.Mapped = true,
		});
		LoadWarden();
		m_Ready = true;
	}

	MenuBackdrop::~MenuBackdrop()
	{
		m_Device.WaitIdle();
		for (const std::unique_ptr<Rdn::Pipeline>& pipeline : m_Pipelines)
			m_Allocator.ReleaseResource(pipeline.get());
		for (const std::unique_ptr<Rdn::Shader>& shader : m_Shaders)
			m_Allocator.ReleaseResource(shader.get());
		if (m_Nebula)
			m_Allocator.ReleaseResource(m_Nebula.get());
		if (m_Sampler)
			m_Allocator.ReleaseResource(m_Sampler.get());
		if (m_ClampSampler)
			m_Allocator.ReleaseResource(m_ClampSampler.get());
		if (m_Buffer)
			m_Allocator.ReleaseResource(m_Buffer.get());
		if (m_Warden)
			m_Allocator.ReleaseResource(m_Warden.get());
	}

	Rdn::Pipeline* MenuBackdrop::CreatePipeline(const char* vertex, const char* fragment, Rdn::Format format, Rdn::BlendMode blend, Rdn::ColorComponent mask)
	{
		Rdn::ShaderDesc vertexDesc;
		Rdn::ShaderDesc fragmentDesc;
		if (!Rdn::ShaderCompiler::Compile(vertex, vertexDesc) || !Rdn::ShaderCompiler::Compile(fragment, fragmentDesc))
			return nullptr;
		Rdn::Shader* stages[2];
		for (Rdn::ShaderDesc* desc : { &vertexDesc, &fragmentDesc })
		{
			m_Shaders.push_back(std::make_unique<Rdn::Shader>());
			m_Allocator.CreateShader(m_Shaders.back().get(), *desc);
			stages[desc == &vertexDesc ? 0 : 1] = m_Shaders.back().get();
		}
		m_Pipelines.push_back(std::make_unique<Rdn::Pipeline>());
		const bool descriptors = std::string_view(fragment).find("CosmosBake") == std::string_view::npos && std::string_view(fragment).find("CosmosVeil") == std::string_view::npos;
		m_Allocator.CreatePipeline(m_Pipelines.back().get(), Rdn::PipelineDesc{
			.ShaderStages = { stages[0], stages[1] },
			.CullMode = Rdn::CullMode::None,
			.DepthTest = false,
			.DepthWrite = false,
			.Blend = blend,
			.ColorAttachmentFormats = { format },
			.ColorWriteMasks = { mask },
			.PushDescriptorSets = descriptors ? std::vector<uint32_t>{ 0 } : std::vector<uint32_t>{},
		});
		return m_Pipelines.back().get();
	}

	void MenuBackdrop::LoadWarden()
	{
		struct Header
		{
			char Magic[4];
			uint32_t Count;
			float Low[3];
			float Extent[3];
			float Eyes[6];
			float Joints[18];
		} header{};
		float hands[24]{};
		std::vector<uint32_t> words;
		std::ifstream file("res/Cosmos/Warden.bin", std::ios::binary);
		if (file.read(reinterpret_cast<char*>(&header), sizeof(header)) && std::memcmp(header.Magic, "WRD3", 4) == 0 && header.Count > 0 && file.read(reinterpret_cast<char*>(hands), sizeof(hands)))
		{
			words.resize(size_t(header.Count) * 3);
			if (!file.read(reinterpret_cast<char*>(words.data()), std::streamsize(words.size() * sizeof(uint32_t))))
				words.clear();
		}
		if (words.empty())
		{
			RDN_LOG_WARNING("res/Cosmos/Warden.bin is missing or damaged, the menu shows no warden");
			words.assign(3, 0u);
		}
		else
		{
			m_WardenCount = header.Count;
			m_WardenLow = glm::vec3(header.Low[0], header.Low[1], header.Low[2]);
			m_WardenExtent = glm::vec3(header.Extent[0], header.Extent[1], header.Extent[2]);
			m_WardenEyes[0] = glm::vec3(header.Eyes[0], header.Eyes[1], header.Eyes[2]);
			m_WardenEyes[1] = glm::vec3(header.Eyes[3], header.Eyes[4], header.Eyes[5]);
			for (int side = 0; side < 2; side++)
			{
				for (int joint = 0; joint < 3; joint++)
					m_WardenJoints[side][joint] = glm::vec3(header.Joints[side * 9 + joint * 3], header.Joints[side * 9 + joint * 3 + 1], header.Joints[side * 9 + joint * 3 + 2]);
				const float* h = hands + side * 12;
				m_WardenHands[side] = { glm::vec3(h[0], h[1], h[2]), glm::normalize(glm::vec3(h[3], h[4], h[5])), glm::normalize(glm::vec3(h[6], h[7], h[8])), h[9], h[10] };
			}
			m_WardenStars = std::min(m_WardenStars, m_WardenCount);
		}
		const uint64_t size = words.size() * sizeof(uint32_t);
		m_Warden = std::make_unique<Rdn::GpuBuffer>();
		m_Allocator.CreateGpuBuffer(m_Warden.get(), Rdn::GpuBufferDesc{ .Size = size, .UsageFlags = Rdn::BufferUsage::StorageBuffer | Rdn::BufferUsage::TransferDst, .MemoryProperty = Rdn::MemoryProperty::DeviceLocal });
		Rdn::GpuBuffer staging;
		m_Allocator.CreateGpuBuffer(&staging, Rdn::GpuBufferDesc{ .Size = size, .UsageFlags = Rdn::BufferUsage::TransferSrc, .MemoryProperty = Rdn::MemoryProperty::HostVisible | Rdn::MemoryProperty::HostCoherent, .Mapped = true });
		staging.Write(words.data(), size);
		m_Device.ImmediateSubmit([&](Rdn::CommandBuffer& cmd) { cmd.CopyBuffer(staging.GetHandle(), m_Warden->GetHandle(), size); });
		m_Allocator.ReleaseResource(&staging);
	}

	void MenuBackdrop::PoseWarden(float time, float tour, float lean, glm::vec4* rows, glm::vec4* curls) const
	{
		const auto store = [rows](int bone, const glm::mat4& m)
		{
			for (int row = 0; row < 3; row++)
				rows[bone * 3 + row] = glm::vec4(m[0][row], m[1][row], m[2][row], m[3][row]);
		};
		const auto arc = [](glm::vec3 from, glm::vec3 to)
		{
			from = glm::normalize(from);
			to = glm::normalize(to);
			const glm::vec3 axis = glm::cross(from, to);
			const float length = glm::length(axis);
			if (length < 1e-5f)
				return glm::mat4(1.0f);
			return glm::rotate(glm::mat4(1.0f), std::atan2(length, glm::dot(from, to)), axis / length);
		};
		const auto basis = [](glm::vec3 along, glm::vec3 palm)
		{
			along = glm::normalize(along);
			palm = glm::normalize(palm - along * glm::dot(palm, along));
			return glm::mat3(glm::cross(along, palm), along, palm);
		};
		store(0, glm::mat4(1.0f));
		const glm::vec3 up(0.0f, std::cos(lean), std::sin(lean));
		const glm::vec3 level(0.0f, -std::sin(lean), std::cos(lean));
		for (int side = 0; side < 2; side++)
		{
			const float sign = side == 0 ? -1.0f : 1.0f;
			const float phase = float(side) * 1.9f;
			const glm::vec3 inward(-sign, 0.0f, 0.0f);
			const WardenHand& rest = m_WardenHands[side];
			const glm::vec3 palmUp = glm::normalize(up + inward * HandTilt + level * HandFacing);
			const glm::vec3 reach = glm::normalize(level * HandReach.x + inward * HandReach.y);
			const glm::mat3 turn = basis(reach, palmUp) * glm::transpose(basis(rest.Along, rest.Palm));
			glm::vec3 centre = HandCentre + glm::vec3(0.006f * std::sin(time * 0.37f + phase), 0.008f * std::sin(time * 0.43f + phase), 0.005f * std::sin(time * 0.29f + phase));
			centre.x *= sign;
			centre.x += sign * 0.01f * tour;
			const glm::vec3 wrist = centre - reach * (rest.Middle * HandScale);
			glm::mat4 hand = glm::translate(glm::mat4(1.0f), wrist) * glm::mat4(turn) * glm::scale(glm::mat4(1.0f), glm::vec3(HandScale)) * glm::translate(glm::mat4(1.0f), -rest.Wrist);

			const glm::vec3 shoulder = m_WardenJoints[side][0];
			const glm::vec3 elbowRest = m_WardenJoints[side][1];
			const glm::vec3 wristRest = m_WardenJoints[side][2];
			const float upperLength = glm::length(elbowRest - shoulder);
			const float foreLength = glm::length(wristRest - elbowRest);
			const glm::vec3 toWrist = wrist - shoulder;
			const float distance = std::max(glm::length(toWrist), 1e-4f);
			const float stretch = std::max(1.0f, distance / ((upperLength + foreLength) * 0.995f));
			const float a = upperLength * stretch;
			const float b = foreLength * stretch;
			const glm::vec3 direction = toWrist / distance;
			const glm::vec3 pole = glm::vec3(sign * 0.6f, -0.8f, -0.2f);
			const glm::vec3 bend = glm::normalize(pole - direction * glm::dot(pole, direction));
			const float along = (a * a - b * b + distance * distance) / (2.0f * distance);
			const glm::vec3 elbow = shoulder + direction * along + bend * std::sqrt(std::max(a * a - along * along, 0.0f));
			const glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(stretch));
			const int base = 1 + side * 3;
			store(base, glm::translate(glm::mat4(1.0f), shoulder) * arc(elbowRest - shoulder, elbow - shoulder) * scale * glm::translate(glm::mat4(1.0f), -shoulder));
			store(base + 1, glm::translate(glm::mat4(1.0f), elbow) * arc(wristRest - elbowRest, wrist - elbow) * scale * glm::translate(glm::mat4(1.0f), -elbowRest));
			store(base + 2, hand);

			const float breathe = std::sin(time * 0.5f + phase);
			const float curl = HandCurl.x + (HandCurl.y - HandCurl.x) * tour + 0.07f * breathe;
			curls[side * 2] = glm::vec4(rest.Wrist + rest.Along * rest.Knuckle, curl);
			curls[side * 2 + 1] = glm::vec4(glm::normalize(glm::cross(rest.Along, rest.Palm)), 0.0f);
		}
	}

	void MenuBackdrop::RenderVeil(Rdn::RenderGraph& graph, Rdn::Extent2D extent, float time)
	{
		m_Light = {};
		if (!m_Ready || extent.Width == 0 || extent.Height == 0)
			return;
		const Rdn::RGImage veil = graph.CreateImage("Pause veil", Rdn::RGImageDesc{ .Size = { std::max(extent.Width / 4u, 1u), std::max(extent.Height / 4u, 1u), 1 }, .Format = HdrFormat });
		const glm::vec4 constants(time, float(extent.Width) / float(extent.Height), m_Quality, 0.0f);
		graph.AddPass("Pause veil", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(veil, Rdn::AttachmentLoadOp::DontCare, Rdn::AttachmentStoreOp::Store);
			},
			[this, constants](Rdn::CommandBuffer& cmd, const Rdn::PassContext&)
			{
				cmd.BindPipeline(m_Veil->GetHandle());
				cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_Veil->GetLayout(), &constants, sizeof(constants));
				cmd.Draw(3, 1, 0, 0);
			});
		m_Light = veil;
	}

	MenuBackdrop::Camera MenuBackdrop::Blend(const Camera& from, const Camera& to, float t) const
	{
		const float s = Smoother(t);
		Camera camera;
		camera.Yaw = from.Yaw + std::remainder(to.Yaw - from.Yaw, 6.2831853f) * s;
		camera.Pitch = std::lerp(from.Pitch, to.Pitch, s);
		camera.Distance = std::exp(std::lerp(std::log(from.Distance), std::log(to.Distance), s));
		camera.Target = glm::mix(from.Target, to.Target, s);
		camera.Roll = std::lerp(from.Roll, to.Roll, s);
		camera.Frame = glm::mix(from.Frame, to.Frame, s);
		camera.Tint = glm::mix(from.Tint, to.Tint, s);
		camera.Exposure = std::lerp(from.Exposure, to.Exposure, s);
		return camera;
	}

	void MenuBackdrop::UpdateCamera(const BackdropView& view, float dt)
	{
		const CosmosStation& station = CosmosStations[size_t(view.Mood)];
		const Camera goal{ station.Yaw, station.Pitch, station.Distance, station.Target, station.Roll, station.Frame, station.Tint, station.Exposure };
		if (m_LastTime < 0.0f)
		{
			m_Mood = view.Mood;
			m_From = goal;
			m_Shown = goal;
			m_Flight = 1.0f;
			return;
		}
		if (view.Mood != m_Mood)
		{
			m_Mood = view.Mood;
			m_From = m_Shown;
			m_Flight = 0.0f;
		}
		m_Flight = std::min(m_Flight + dt / FlightSeconds, 1.0f);
		m_Shown = Blend(m_From, goal, m_Flight);
	}

	bool MenuBackdrop::Render(Rdn::RenderGraph& graph, Rdn::RGImage target, Rdn::Extent2D extent, uint32_t frameIndex, const BackdropView& view)
	{
		m_Light = {};
		if (!m_Ready || extent.Width == 0 || extent.Height == 0)
			return false;
		const float dt = m_LastTime < 0.0f ? 0.0f : std::clamp(view.Time - m_LastTime, 0.0f, 0.1f);
		UpdateCamera(view, dt);
		m_LastTime = view.Time;

		const Rdn::Extent2D size{ std::max(uint32_t(float(extent.Width) * m_Scale), 1u), std::max(uint32_t(float(extent.Height) * m_Scale), 1u) };
		const float pixelsPerUnit = view.UiScale * float(size.Width) / float(extent.Width);
		const glm::vec2 mouse = view.Mouse * pixelsPerUnit;
		const bool mouseValid = view.Mouse.x > -1e3f;
		if (mouseValid && m_Mouse.x > -1e3f)
			m_Stir += glm::length(mouse - m_Mouse) * 0.0035f / std::max(pixelsPerUnit, 0.1f);
		m_Stir = std::min(m_Stir * std::exp(-dt * 1.6f), 1.6f);
		m_Mouse = mouseValid ? mouse : glm::vec2(-1e4f);

		const float dive = 1.0f - std::exp(-view.Dive * 0.75f);
		Camera camera = m_Shown;
		camera.Yaw += view.Time * 0.0085f + view.Parallax.x * 0.05f + view.Dive * 0.22f;
		camera.Yaw += view.Orbit.x + view.Tour * 0.16f * std::sin(view.Time * 0.045f);
		const float homeYaw = CosmosStations[0].Yaw + view.Time * 0.0085f;
		const float homePitch = CosmosStations[0].Pitch;
		const glm::vec3 behind = -WardenDepth * glm::vec3(std::cos(homePitch) * std::sin(homeYaw), std::sin(homePitch), std::cos(homePitch) * std::cos(homeYaw));
		const float lean = homePitch;
		const float wardenRoll = CosmosStations[0].Roll * (1.0f - 0.6f * view.Tour);
		camera.Pitch = std::lerp(camera.Pitch, homePitch + 0.05f * std::sin(view.Time * 0.043f), view.Tour) + view.Orbit.y;
		camera.Distance = std::lerp(camera.Distance, 2.2f + 0.12f * std::sin(view.Time * 0.031f), view.Tour);
		camera.Frame = glm::mix(camera.Frame, glm::vec2(0.0f, 0.2f), view.Tour);
		camera.Roll *= 1.0f - 0.6f * view.Tour;
		camera.Pitch = std::clamp(camera.Pitch + 0.03f * std::sin(view.Time * 0.071f) - view.Parallax.y * 0.03f, -1.35f, 1.35f);
		camera.Distance *= 1.0f + 0.025f * std::sin(view.Time * 0.053f + 1.3f);
		camera.Distance *= std::lerp(1.0f, 0.16f, dive);
		camera.Target = glm::mix(camera.Target, glm::vec3(0.0f), dive);
		camera.Frame = glm::mix(camera.Frame, glm::vec2(0.0f), dive);
		const glm::vec3 position = camera.Target + camera.Distance * glm::vec3(std::cos(camera.Pitch) * std::sin(camera.Yaw), std::sin(camera.Pitch), std::cos(camera.Pitch) * std::cos(camera.Yaw));
		const glm::vec3 forward = glm::normalize(camera.Target - position);
		glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
		glm::vec3 up = glm::cross(right, forward);
		const float roll = camera.Roll + view.Parallax.x * 0.015f;
		const glm::vec3 rolledRight = right * std::cos(roll) + up * std::sin(roll);
		up = glm::cross(rolledRight, forward);
		right = rolledRight;
		const float tanY = std::tan(FieldOfView * 0.5f) * (1.0f + 0.35f * dive);
		const float aspect = float(size.Width) / float(size.Height);
		const float tanX = tanY * aspect;

		glm::mat4 viewMatrix(1.0f);
		viewMatrix[0] = glm::vec4(right.x, up.x, -forward.x, 0.0f);
		viewMatrix[1] = glm::vec4(right.y, up.y, -forward.y, 0.0f);
		viewMatrix[2] = glm::vec4(right.z, up.z, -forward.z, 0.0f);
		viewMatrix[3] = glm::vec4(-glm::dot(right, position), -glm::dot(up, position), glm::dot(forward, position), 1.0f);
		glm::mat4 projection(0.0f);
		projection[0][0] = 1.0f / tanX;
		projection[1][1] = -1.0f / tanY;
		projection[2][0] = -camera.Frame.x;
		projection[2][1] = -camera.Frame.y;
		projection[2][3] = -1.0f;
		projection[2][2] = 0.0f;
		projection[3][2] = 0.001f;
		const glm::mat4 viewProjection = projection * viewMatrix;
		if (!m_HasPrevious)
			m_PreviousViewProjection = viewProjection;

		const float flash = view.Dive > 0.0f ? std::clamp((view.Dive - 1.6f) / 1.2f, 0.0f, 1.0f) : 0.0f;
		Frame frame{
			.ViewProjection = viewProjection,
			.PreviousViewProjection = m_PreviousViewProjection,
			.Camera = glm::vec4(position, view.Time),
			.Right = glm::vec4(right, tanX),
			.Up = glm::vec4(up, tanY),
			.Forward = glm::vec4(forward, pixelsPerUnit),
			.Screen = glm::vec4(float(size.Width), float(size.Height), 1.0f / float(size.Width), 1.0f / float(size.Height)),
			.Lens = glm::vec4(camera.Frame, camera.Exposure * (1.0f - 0.35f * dive), view.Fade),
			.Mouse = glm::vec4(m_Mouse, m_Stir, mouseValid ? 1.0f : 0.0f),
			.Tint = glm::vec4(camera.Tint, dive),
			.Galaxy = glm::vec4(view.Time * 0.004f, m_Quality, flash, view.Time),
			.Title = glm::vec4(view.TitleTime, view.TitleShown, 0.0f, 0.0f),
			.Counts = glm::vec4(float(m_GalaxyStars), float(m_DustMotes), float(m_TitleMotes), float(m_BakedBands) / float(m_BakeBands)),
			.Cloud = glm::vec4((view.Cloud.x + view.Cloud.z * 0.5f) * view.UiScale / float(extent.Width), (view.Cloud.y + view.Cloud.w * 0.5f) * view.UiScale / float(extent.Height), view.Cloud.z * 0.5f * view.UiScale / float(extent.Height), view.Cloud.w * 0.5f * view.UiScale / float(extent.Height)),
			.CloudState = glm::vec4(0.0f, view.Time, view.PageShown, 0.0f),
		};
		m_PreviousViewProjection = viewProjection;
		m_HasPrevious = true;
		const float wardenShown = m_WardenCount > 0 ? (1.0f - dive) * std::lerp(std::lerp(0.55f, 0.3f, view.PageShown), 1.0f, view.Tour) : 0.0f;
		frame.Warden[0] = glm::vec4(behind, WardenScale * (1.0f + 0.01f * std::sin(view.Time * 0.52f)));
		frame.Warden[1] = glm::vec4(std::cos(homeYaw), std::sin(homeYaw), wardenShown, glm::smoothstep(0.8f, 5.0f, view.TitleTime));
		frame.Warden[2] = glm::vec4(m_WardenLow, float(m_WardenCount));
		frame.Warden[3] = glm::vec4(m_WardenExtent, lean);
		frame.Warden[4] = glm::vec4(m_WardenEyes[0], wardenRoll);
		frame.Warden[5] = glm::vec4(m_WardenEyes[1], 0.0f);
		const uint32_t wardenStars = wardenShown > 0.002f ? std::max(uint32_t(float(m_WardenStars) * std::lerp(1.0f, 0.35f, view.PageShown * (1.0f - view.Tour))), 4u) : 0u;
		frame.Warden[6] = glm::vec4(view.Tour, wardenStars > 0 ? float(m_WardenCount) / float(wardenStars) : 1.0f, 0.0f, 0.0f);
		PoseWarden(view.Time, view.Tour, lean, frame.WardenBones, frame.Warden + 7);

		Glyph glyphs[MaxGlyphs];
		uint32_t glyphCount = 0;
		float area = 0.0f;
		for (const UiPrimitive& primitive : view.Title)
		{
			if (primitive.Info.x != uint32_t(UiMode::Glyph) || glyphCount >= MaxGlyphs)
				continue;
			const glm::vec2 center = glm::vec2(primitive.Rect) * pixelsPerUnit;
			const glm::vec2 half = glm::vec2(primitive.Rect.z, primitive.Rect.w) * pixelsPerUnit;
			area += half.x * half.y;
			glyphs[glyphCount++] = { glm::vec4(center, half), primitive.Uv, glm::vec4(area, 0.0f, 0.0f, 0.0f) };
		}
		frame.Title.z = float(glyphCount);
		frame.Title.w = area;
		uint8_t* mapped = static_cast<uint8_t*>(m_Buffer->GetMappedPtr()) + m_Buffer->GetOffset(frameIndex);
		std::memcpy(mapped, &frame, sizeof(Frame));
		if (glyphCount > 0)
			std::memcpy(mapped + sizeof(Frame), glyphs, glyphCount * sizeof(Glyph));
		const uint64_t bufferOffset = m_Buffer->GetOffset(frameIndex);
		const uint64_t bufferSize = sizeof(Frame) + MaxGlyphs * sizeof(Glyph);

		const Rdn::RGImage nebula = graph.ImportImage("Cosmos nebula", m_Nebula.get());
		if (m_BakedBands < m_BakeBands)
		{
			const uint32_t band = m_BakedBands++;
			const uint32_t bands = m_BakeBands;
			const uint32_t nebulaSize = m_NebulaSize;
			graph.AddPass("Cosmos nebula bake", Rdn::PassType::Graphics,
				[&](Rdn::PassBuilder& builder)
				{
					builder.AddColorAttachment(nebula, band == 0 ? Rdn::AttachmentLoadOp::Clear : Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store, { 0.0f, 0.0f, 0.0f, 0.0f });
				},
				[this, band, bands, nebulaSize](Rdn::CommandBuffer& cmd, const Rdn::PassContext&)
				{
					const uint32_t height = (nebulaSize + bands - 1) / bands;
					cmd.SetScissor(0, int32_t(band * height), nebulaSize, height);
					const PassConstants constants{ 0, 0, band, bands };
					cmd.BindPipeline(m_Bake->GetHandle());
					cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_Bake->GetLayout(), &constants, sizeof(constants));
					cmd.Draw(3, 1, 0, 0);
				});
		}

		const Rdn::RGImage hdr = graph.CreateImage("Cosmos", Rdn::RGImageDesc{ .Size = { size.Width, size.Height, 1 }, .Format = HdrFormat });
		const auto frameWrite = [this, bufferOffset, bufferSize]()
		{
			return Rdn::BufferWrite(0, Rdn::DescriptorType::StorageBuffer, m_Buffer->GetHandle(), bufferOffset, bufferSize);
		};
		graph.AddPass("Cosmos sky", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(hdr, Rdn::AttachmentLoadOp::DontCare, Rdn::AttachmentStoreOp::Store);
				builder.UseImage(nebula, Rdn::RGImageUsage::Sampled);
			},
			[this, nebula, frameWrite](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				cmd.BindPipeline(m_Sky->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Sky->GetLayout(), 0, Rdn::DescriptorWrite()
					.AddWrite(frameWrite())
					.AddWrite(Rdn::ImageWrite(1, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(nebula), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_Sampler->GetHandle())));
				cmd.Draw(3, 1, 0, 0);
			});

		const uint32_t titleMotes = glyphCount > 0 ? m_TitleMotes : 0u;
				const auto pointsPass = [&](const char* name, uint32_t kinds)
		{
			graph.AddPass(name, Rdn::PassType::Graphics,
				[&](Rdn::PassBuilder& builder)
				{
					builder.AddColorAttachment(hdr, Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store);
				},
				[this, frameWrite, titleMotes, wardenStars, kinds](Rdn::CommandBuffer& cmd, const Rdn::PassContext&)
				{
					cmd.BindPipeline(m_Points->GetHandle());
					cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Points->GetLayout(), 0, Rdn::DescriptorWrite()
						.AddWrite(frameWrite())
						.AddWrite(Rdn::ImageWrite(1, Rdn::DescriptorType::CombinedImageSampler, m_Atlas->GetImageView(), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_ClampSampler->GetHandle()))
						.AddWrite(Rdn::BufferWrite(2, Rdn::DescriptorType::StorageBuffer, m_Warden->GetHandle(), 0, m_Warden->GetSize())));
					const uint32_t counts[] = { m_GalaxyStars, m_DustMotes, titleMotes, wardenStars };
					for (uint32_t kind = 0; kind < 4; kind++)
					{
						if ((kinds & (1u << kind)) == 0 || counts[kind] == 0)
							continue;
						const PassConstants constants{ kind, counts[kind], 0, 1 };
						cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_Points->GetLayout(), &constants, sizeof(constants));
						cmd.Draw(6, counts[kind], 0, 0);
					}
				});
		};
		pointsPass("Cosmos stars", 1u);
		pointsPass("Cosmos dust", 10u);

		const Rdn::RGImageDesc quarter{ .Size = { std::max(size.Width / 4u, 1u), std::max(size.Height / 4u, 1u), 1 }, .Format = HdrFormat };
		const auto bloomPass = [&](const char* name, Rdn::RGImage source, Rdn::RGImage destination)
		{
			graph.AddPass(name, Rdn::PassType::Graphics,
				[&](Rdn::PassBuilder& builder)
				{
					builder.AddColorAttachment(destination, Rdn::AttachmentLoadOp::DontCare, Rdn::AttachmentStoreOp::Store);
					builder.UseImage(source, Rdn::RGImageUsage::Sampled);
				},
				[this, source, frameWrite](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
				{
					cmd.BindPipeline(m_Bloom->GetHandle());
					cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Bloom->GetLayout(), 0, Rdn::DescriptorWrite()
						.AddWrite(frameWrite())
						.AddWrite(Rdn::ImageWrite(1, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(source), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_ClampSampler->GetHandle())));
					cmd.Draw(3, 1, 0, 0);
				});
		};
		const Rdn::RGImage glare = graph.CreateImage("Cosmos glare", quarter);
		const Rdn::RGImage bloom = graph.CreateImage("Cosmos bloom", quarter);
		bloomPass("Cosmos glare", hdr, glare);
		bloomPass("Cosmos bloom", glare, bloom);
		if (titleMotes > 0)
			pointsPass("Cosmos title", 4u);

		graph.AddPass("Cosmos composite", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(target, Rdn::AttachmentLoadOp::DontCare, Rdn::AttachmentStoreOp::Store);
				builder.UseImage(hdr, Rdn::RGImageUsage::Sampled);
				builder.UseImage(bloom, Rdn::RGImageUsage::Sampled);
			},
			[this, hdr, bloom, frameWrite](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				cmd.BindPipeline(m_Composite->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Composite->GetLayout(), 0, Rdn::DescriptorWrite()
					.AddWrite(frameWrite())
					.AddWrite(Rdn::ImageWrite(1, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(hdr), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_ClampSampler->GetHandle()))
					.AddWrite(Rdn::ImageWrite(2, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(bloom), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_ClampSampler->GetHandle())));
				cmd.Draw(3, 1, 0, 0);
			});

		const Rdn::RGImage light = graph.CreateImage("Cosmos light", Rdn::RGImageDesc{ .Size = { std::max(extent.Width / 8u, 1u), std::max(extent.Height / 8u, 1u), 1 }, .Format = LightFormat });
		graph.AddPass("Cosmos light", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(light, Rdn::AttachmentLoadOp::DontCare, Rdn::AttachmentStoreOp::Store);
				builder.UseImage(hdr, Rdn::RGImageUsage::Sampled);
			},
			[this, hdr, frameWrite](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				cmd.BindPipeline(m_LightPass->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_LightPass->GetLayout(), 0, Rdn::DescriptorWrite()
					.AddWrite(frameWrite())
					.AddWrite(Rdn::ImageWrite(1, Rdn::DescriptorType::CombinedImageSampler, context.GetImageView(hdr), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_ClampSampler->GetHandle())));
				cmd.Draw(3, 1, 0, 0);
			});
		m_Light = light;
		return true;
	}
}

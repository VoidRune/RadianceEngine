#pragma once
#include "Rendering/GBuffer.h"
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace Game
{
	inline constexpr Rdn::Format ParticleLightFormat = Rdn::Format::R16G16B16A16_Sfloat;

	struct WorldCamera
	{
		glm::mat4 View{ 1.0f };
		glm::mat4 Projection{ 1.0f };
		glm::mat4 ViewProjection{ 1.0f };
		glm::vec3 Eye{ 0.0f };
		float Near = 0.5f;
		float Far = 200.0f;
	};

	enum class ParticleShape : uint8_t
	{
		Glow,
		Spark,
		Smoke,
		Ring,
		Star,
		Shard,
		Debris,
		Flame,
		Orb,
		Lightning,
		Rune,
		Slash,
		Snowflake,
		Beam,
	};

	enum class DecalShape : uint8_t
	{
		Runes,
		Sector,
		Lane,
		Ring,
		Glow,
		Scorch,
		Frost,
		Cracks,
		Fracture,
	};

	enum class ParticleAlign : uint8_t
	{
		Billboard,
		Velocity,
		Ground,
	};

	enum class SpawnShape : uint8_t
	{
		Sphere,
		Disc,
		Ring,
		Segment,
	};

	struct ParticleMaterial
	{
		glm::vec4 Colors[4]{};
		glm::vec4 ColorTimes{ 0.0f, 0.33f, 0.66f, 1.0f };
		glm::vec4 Sizes{ 1.0f };
		glm::vec4 SizeTimes{ 0.0f, 0.33f, 0.66f, 1.0f };
		glm::vec4 Physics{ 0.0f };
		glm::vec4 Collision{ 0.0f };
		glm::vec4 Look{ 0.0f };
		glm::vec4 Motion{ 0.0f, 0.25f, 0.0f, 0.0f };
	};
	static_assert(sizeof(ParticleMaterial) == 176);

	struct ParticleSpawn
	{
		uint32_t Material = 0;
		uint32_t Count = 0;
		SpawnShape Shape = SpawnShape::Sphere;
		bool Outward = false;
		glm::vec3 Origin{ 0.0f };
		float Radius = 0.0f;
		glm::vec3 Direction{ 0.0f, 1.0f, 0.0f };
		float Cone = 3.14159265f;
		glm::vec3 Velocity{ 0.0f };
		glm::vec3 Segment{ 0.0f };
		glm::vec2 Speed{ 0.0f };
		glm::vec2 Life{ 0.5f, 1.0f };
		glm::vec2 Size{ 0.1f, 0.2f };
		glm::vec3 Tint{ 1.0f };
	};

	struct ParticleSprite
	{
		glm::vec3 Position{ 0.0f };
		float Size = 0.2f;
		glm::vec4 Color{ 1.0f };
		ParticleShape Shape = ParticleShape::Glow;
		ParticleAlign Align = ParticleAlign::Billboard;
		float Rotation = 0.0f;
		float Additive = 1.0f;
		float Emission = 1.0f;
		float Softness = 0.2f;
		glm::vec3 Axis{ 0.0f };
		float Stretch = 0.0f;
		glm::vec4 Params{ 0.0f };
		uint32_t Seed = 0;
	};

	struct GroundDecal
	{
		glm::vec3 Center{ 0.0f };
		float Yaw = 0.0f;
		glm::vec2 Extent{ 1.0f };
		float Depth = 0.6f;
		DecalShape Shape = DecalShape::Glow;
		glm::vec4 Color{ 1.0f };
		glm::vec4 Tint{ 0.0f };
		float Roughness = -1.0f;
		glm::vec4 Params{ 0.0f };
		uint32_t Seed = 0;
	};

	class ParticleSystem
	{
	public:
		static constexpr uint32_t Capacity = 65536;
		static constexpr uint32_t MaxSpawnsPerFrame = 16384;
		static constexpr uint32_t MaxBatchesPerFrame = 2048;
		static constexpr uint32_t MaxSprites = 4096;
		static constexpr uint32_t MaxDecals = 512;

		ParticleSystem(Rdn::Device& device, Rdn::ResourceAllocator& allocator);
		~ParticleSystem();
		ParticleSystem(const ParticleSystem&) = delete;
		ParticleSystem& operator=(const ParticleSystem&) = delete;

		void SetMaterials(std::span<const ParticleMaterial> materials);
		void Spawn(const ParticleSpawn& spawn);
		void BeginFrame();
		void DrawSprite(const ParticleSprite& sprite);
		void DrawDecal(const GroundDecal& decal);
		void Advance(float dt);
		float GetTime() const { return m_Time; }
		void Clear();
		void Upload(Rdn::Extent2D extent, uint32_t frameIndex, const WorldCamera& camera);
		void RenderDecals(Rdn::RenderGraph& graph, const GBuffer& gbuffer, uint32_t frameIndex);
		void Simulate(Rdn::RenderGraph& graph, Rdn::RGImage depth, uint32_t frameIndex);
		void DrawLight(Rdn::RenderGraph& graph, Rdn::RGImage target, Rdn::RGImage depth, uint32_t frameIndex);
		void Draw(Rdn::RenderGraph& graph, Rdn::RGImage scene, Rdn::RGImage emission, Rdn::RGImage depth, uint32_t frameIndex);

		bool IsReady() const { return m_Ready; }
		uint32_t GetQueuedCount() const { return m_QueuedCount; }

	private:
		struct SpawnGpu
		{
			glm::vec4 Origin;
			glm::vec4 Direction;
			glm::vec4 Velocity;
			glm::vec4 Ranges;
			glm::vec4 Tint;
			glm::vec4 Segment;
			glm::uvec4 Info;
		};

		struct FrameGpu
		{
			glm::mat4 ViewProjection;
			glm::mat4 InverseViewProjection;
			glm::vec4 CameraRight;
			glm::vec4 CameraUp;
			glm::vec4 CameraForward;
			glm::vec4 Screen;
			glm::uvec4 Counts;
		};

		struct SpriteGpu
		{
			glm::vec4 PositionSize;
			glm::vec4 Color;
			glm::vec4 AxisStretch;
			glm::vec4 Params;
			glm::vec4 Look;
			glm::uvec4 Info;
		};

		struct DecalGpu
		{
			glm::vec4 CenterYaw;
			glm::vec4 Extent;
			glm::vec4 Color;
			glm::vec4 Tint;
			glm::vec4 Params;
			glm::uvec4 Info;
		};

		struct GraphBuffers
		{
			Rdn::RGBuffer State;
			Rdn::RGBuffer Particles;
			Rdn::RGBuffer Lists;
			Rdn::RGBuffer Materials;
		};

		bool CreatePipelines();
		void ResetBuffers();
		GraphBuffers ImportBuffers(Rdn::RenderGraph& graph) const;
		std::unique_ptr<Rdn::ComputePipeline> CreateCompute(const char* path);
		Rdn::Shader* CompileShader(const char* path);
		std::unique_ptr<Rdn::Pipeline> CreateDraw(Rdn::Shader* vertex, Rdn::Shader* fragment, std::vector<Rdn::Format> formats, std::vector<Rdn::ColorComponent> masks = {}, Rdn::BlendMode blend = Rdn::BlendMode::Premultiplied);
		void RecordDraws(Rdn::CommandBuffer& cmd, const Rdn::PassContext& context, Rdn::RGImage depth, const Rdn::Pipeline& particles, const Rdn::Pipeline& sprites, bool light, uint32_t spriteCount, uint32_t frameIndex) const;

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		std::vector<std::unique_ptr<Rdn::Shader>> m_Shaders;
		std::unique_ptr<Rdn::ComputePipeline> m_Emit;
		std::unique_ptr<Rdn::ComputePipeline> m_Prepare;
		std::unique_ptr<Rdn::ComputePipeline> m_Simulate;
		std::unique_ptr<Rdn::ComputePipeline> m_Finish;
		std::unique_ptr<Rdn::Pipeline> m_Draw;
		std::unique_ptr<Rdn::Pipeline> m_SpriteDraw;
		std::unique_ptr<Rdn::Pipeline> m_LightDraw;
		std::unique_ptr<Rdn::Pipeline> m_SpriteLightDraw;
		std::unique_ptr<Rdn::Pipeline> m_DecalDraw;
		std::unique_ptr<Rdn::GpuBuffer> m_State;
		std::unique_ptr<Rdn::GpuBuffer> m_Particles;
		std::unique_ptr<Rdn::GpuBuffer> m_Lists;
		std::unique_ptr<Rdn::GpuBuffer> m_Materials;
		std::unique_ptr<Rdn::GpuRingBuffer> m_Frame;
		std::unique_ptr<Rdn::GpuRingBuffer> m_SpawnBuffer;
		std::unique_ptr<Rdn::GpuRingBuffer> m_SpriteBuffer;
		std::unique_ptr<Rdn::GpuRingBuffer> m_DecalBuffer;
		std::unique_ptr<Rdn::Sampler> m_DepthSampler;
		std::vector<SpawnGpu> m_Pending;
		std::vector<SpriteGpu> m_Sprites;
		std::vector<DecalGpu> m_Decals;
		uint32_t m_FrameSpawns = 0;
		uint32_t m_FrameBatches = 0;
		uint32_t m_FrameSprites = 0;
		uint32_t m_FrameDecals = 0;
		bool m_Uploaded = false;
		bool m_Simulated = false;
		uint32_t m_QueuedCount = 0;
		uint32_t m_Seed = 1;
		uint32_t m_Parity = 0;
		float m_Time = 0.0f;
		float m_PendingDt = 0.0f;
		bool m_ResetRequested = false;
		bool m_Ready = false;
	};
}

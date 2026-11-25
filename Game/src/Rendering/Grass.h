#pragma once
#include "Rendering/GBuffer.h"
#include "Rendering/Lighting.h"
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <vector>

namespace Game
{
	enum class GrassQuality : uint8_t
	{
		Off,
		Low,
		High,
		Count,
	};

	inline constexpr const char* GrassQualityNames[] = { "off", "low", "high" };
	inline constexpr float GrassPushRadius = 0.6f;
	static_assert(std::size(GrassQualityNames) == size_t(GrassQuality::Count));

	struct GrassPaving
	{
		const Rdn::GpuImage* Image = nullptr;
		const Rdn::GpuImage* Cells = nullptr;
		float Scale = 1.0f;
		float Joint = 0.33f;
	};

	struct GrassField
	{
		std::vector<glm::vec4> Texels;
		std::vector<uint32_t> Flora;
		std::vector<uint32_t> Paving;
		std::optional<uint32_t> PavingMaterial;
		float PavingJoint = 0.33f;
		int32_t Columns = 0;
		int32_t Rows = 0;
		glm::vec2 Origin{ 0.0f };
		float Step = 0.25f;

		bool IsEmpty() const { return Texels.empty(); }
	};

	enum class GrassStampKind : uint8_t
	{
		Burn,
		Frost,
		Flatten,
		Trail,
		Wave,
	};

	struct GrassStamp
	{
		GrassStampKind Kind = GrassStampKind::Flatten;
		glm::vec3 Center{ 0.0f };
		glm::vec2 Extent{ 1.0f };
		float Yaw = 0.0f;
		float Strength = 1.0f;
		float Heat = 0.0f;
		float Width = 0.0f;
		glm::vec2 Direction{ 0.0f };
		float Facing = 0.0f;
		float HalfArc = 4.0f;
	};

	class GrassRenderer
	{
	public:
		static constexpr uint32_t MaxPushers = 32;
		static constexpr uint32_t MaxStamps = 96;
		static constexpr uint32_t Kinds = 6;

		GrassRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator);
		~GrassRenderer();
		GrassRenderer(const GrassRenderer&) = delete;
		GrassRenderer& operator=(const GrassRenderer&) = delete;

		void SetField(const GrassField& field, const GrassPaving& paving);
		void BeginFrame(float time);
		void Push(const glm::vec3& position, float radius, uint32_t key);
		void Stamp(const GrassStamp& stamp);
		void Render(Rdn::RenderGraph& graph, const GBuffer& gbuffer, uint32_t frameIndex, const WorldCamera& camera, Rdn::Extent2D extent);
		bool HasField() const { return m_FieldImage != nullptr; }
		uint32_t GetCandidateCount() const { return m_LastCandidates; }

		GrassQuality Quality = GrassQuality::High;

	private:
		struct Pusher
		{
			glm::vec4 PositionRadius;
			uint32_t Key = 0;
		};

		struct FrameGpu
		{
			glm::mat4 ViewProjection;
			glm::vec4 Eye;
			glm::ivec4 Region;
			std::array<glm::uvec4, Kinds> Draws;
			glm::uvec4 Counts;
			glm::vec4 Grid;
			glm::vec4 Field;
			glm::vec4 FieldSize;
			glm::vec4 Effects;
			glm::vec4 EffectSize;
			glm::vec4 Wind;
			glm::vec4 Shape;
			glm::vec4 Root;
			glm::vec4 Tip;
			glm::vec4 Dry;
			glm::vec4 Look;
			glm::vec4 Decay;
			glm::vec4 Paving;
			std::array<glm::vec4, MaxPushers> Pushers;
			std::array<glm::vec4, MaxStamps * 3> Stamps;
		};

		void AddTrails(float dt);

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		std::vector<std::unique_ptr<Rdn::Shader>> m_Shaders;
		std::unique_ptr<Rdn::ComputePipeline> m_Generate;
		std::unique_ptr<Rdn::ComputePipeline> m_Reset;
		std::unique_ptr<Rdn::ComputePipeline> m_Finish;
		std::unique_ptr<Rdn::ComputePipeline> m_Effects;
		std::unique_ptr<Rdn::Pipeline> m_Draw;
		std::unique_ptr<Rdn::GpuBuffer> m_Blades;
		std::unique_ptr<Rdn::GpuBuffer> m_Command;
		std::unique_ptr<Rdn::GpuBuffer> m_Indices;
		std::unique_ptr<Rdn::GpuRingBuffer> m_Frame;
		std::unique_ptr<Rdn::GpuImage> m_FieldImage;
		std::unique_ptr<Rdn::GpuImage> m_EffectImage;
		std::unique_ptr<Rdn::GpuImage> m_FloraImage;
		std::unique_ptr<Rdn::GpuImage> m_PavingImage;
		std::unique_ptr<Rdn::Sampler> m_FieldSampler;
		std::unique_ptr<Rdn::Sampler> m_RepeatSampler;
		GrassPaving m_Paving;
		glm::vec2 m_FieldMin{ 0.0f };
		glm::vec2 m_FieldMax{ 0.0f };
		glm::vec2 m_FieldHeights{ 0.0f };
		glm::vec2 m_FieldOrigin{ 0.0f };
		glm::ivec2 m_FieldSize{ 0 };
		glm::ivec2 m_EffectSize{ 0 };
		float m_FieldStep = 0.25f;
		std::vector<Pusher> m_Pushers;
		std::vector<glm::vec3> m_Previous;
		std::vector<GrassStamp> m_Stamps;
		std::array<std::array<glm::uvec2, Kinds>, size_t(GrassQuality::Count)> m_Ranges{};
		float m_Time = 0.0f;
		float m_LastTime = -1.0f;
		float m_Delta = 0.0f;
		uint32_t m_LastCandidates = 0;
		bool m_Ready = false;
	};
}

#pragma once
#include "Rendering/Lighting.h"
#include "Rendering/Particles.h"
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace Game
{
	inline constexpr Rdn::Format GlassPyramidFormat = Rdn::Format::R16G16B16A16_Sfloat;
	inline constexpr Rdn::Format GlassBackFormat = Rdn::Format::R16G16B16A16_Sfloat;
	inline constexpr uint32_t GlassPyramidLevels = 4;

	struct GlassFacet
	{
		glm::vec3 A{ 0.0f };
		glm::vec3 B{ 0.0f };
		glm::vec3 C{ 0.0f };
		glm::vec3 Absorption{ 0.0f };
		float Scatter = 0.0f;
		float Thickness = 0.0f;
		glm::vec3 Edge{ 1.0f };
		float Glint = 1.0f;
		float Ior = 1.5f;
		float Dispersion = 0.0f;
		float Roughness = 0.02f;
		float Floor = 0.0f;
		float Crack = 0.0f;
		glm::vec2 CrackOrigin{ 0.0f };
		float Wedge = 0.0f;
		float Flash = 0.0f;
		float Fade = 1.0f;
		uint32_t Seed = 0;
	};

	inline constexpr size_t EnergyShieldImpacts = 8;

	struct EnergyShield
	{
		glm::vec3 Center{ 0.0f };
		float Radius = 1.0f;
		glm::vec3 Forward{ 0.0f, 0.0f, 1.0f };
		glm::vec3 Right{ 1.0f, 0.0f, 0.0f };
		glm::vec3 Rise{ 0.0f, 1.0f, 0.0f };
		glm::vec3 Color{ 1.0f };
		float Intensity = 1.0f;
		float Lattice = 0.1f;
		float Shatter = 0.0f;
		std::array<glm::vec4, EnergyShieldImpacts> Impacts{};
		uint32_t Seed = 0;
	};

	struct EnergyCell
	{
		glm::vec3 Center{ 0.0f };
		glm::vec3 Right{ 1.0f, 0.0f, 0.0f };
		glm::vec3 Up{ 0.0f, 1.0f, 0.0f };
		glm::vec2 Plane{ 0.0f };
		float Size = 0.1f;
		float Lift = 0.0f;
		float Form = 1.0f;
		float Flare = 0.0f;
		float Crack = 0.0f;
		float Flicker = 1.0f;
		float Fade = 1.0f;
		float Spin = 0.0f;
		uint32_t Shield = 0;
		uint32_t Seed = 0;
		bool Attached = true;
	};

	class GlassRenderer
	{
	public:
		static constexpr uint32_t MaxFacets = 2048;
		static constexpr uint32_t MaxShields = 16;
		static constexpr uint32_t MaxCells = 2048;

		GlassRenderer(Rdn::Device& device, Rdn::ResourceAllocator& allocator);
		~GlassRenderer();
		GlassRenderer(const GlassRenderer&) = delete;
		GlassRenderer& operator=(const GlassRenderer&) = delete;

		void BeginFrame();
		void Draw(const GlassFacet& facet);
		uint32_t AddShield(const EnergyShield& shield);
		void DrawCell(const EnergyCell& cell);
		void Render(Rdn::RenderGraph& graph, Rdn::RGImage scene, Rdn::RGImage emission, Rdn::RGImage depth, Rdn::Extent2D extent, uint32_t frameIndex,
			const WorldCamera& camera, const SceneLighting& lighting, const EnvironmentView& environment, float time);
		uint32_t GetCount() const { return uint32_t(m_Facets.size()); }

	private:
		struct FacetGpu
		{
			glm::vec4 A;
			glm::vec4 B;
			glm::vec4 C;
			glm::vec4 Absorb;
			glm::vec4 Edge;
			glm::vec4 Optics;
			glm::vec4 Crack;
			glm::uvec4 Info;
		};

		struct ShieldGpu
		{
			glm::vec4 Sphere;
			glm::vec4 Forward;
			glm::vec4 Right;
			glm::vec4 Rise;
			glm::vec4 Color;
			std::array<glm::vec4, EnergyShieldImpacts> Impacts;
			glm::uvec4 Info;
		};

		struct CellGpu
		{
			glm::vec4 Center;
			glm::vec4 Right;
			glm::vec4 Up;
			glm::vec4 Plane;
			glm::vec4 State;
			glm::uvec4 Info;
		};

		struct FrameGpu
		{
			glm::mat4 ViewProjection;
			glm::vec4 Eye;
			glm::vec4 Forward;
			glm::vec4 Screen;
			glm::vec4 Region;
			std::array<glm::vec4, GlassPyramidLevels> Levels;
			glm::vec4 Atlas;
			glm::vec4 SunDirection;
			glm::vec4 SunRadiance;
			glm::vec4 SkyRadiance;
			glm::vec4 GroundRadiance;
			glm::vec4 Environment;
		};

		struct PyramidConstants
		{
			glm::ivec4 Target;
			glm::ivec4 Count;
			glm::ivec4 Source;
			glm::ivec4 Bounds;
			glm::vec4 Scale;
		};

		Rdn::Shader* Compile(const char* path, const char* define = nullptr, Rdn::ShaderStage stage = Rdn::ShaderStage::Compute);

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		std::vector<std::unique_ptr<Rdn::Shader>> m_Shaders;
		std::unique_ptr<Rdn::ComputePipeline> m_FirstLevel;
		std::unique_ptr<Rdn::ComputePipeline> m_NextLevel;
		std::unique_ptr<Rdn::Pipeline> m_Back;
		std::unique_ptr<Rdn::Pipeline> m_Front;
		std::unique_ptr<Rdn::Pipeline> m_CrackedFront;
		std::unique_ptr<Rdn::Pipeline> m_Lattice;
		std::unique_ptr<Rdn::GpuRingBuffer> m_Frame;
		std::unique_ptr<Rdn::GpuRingBuffer> m_Buffer;
		std::unique_ptr<Rdn::GpuRingBuffer> m_ShieldBuffer;
		std::unique_ptr<Rdn::GpuRingBuffer> m_CellBuffer;
		std::unique_ptr<Rdn::Sampler> m_LinearSampler;
		std::unique_ptr<Rdn::Sampler> m_PointSampler;
		std::vector<FacetGpu> m_Facets;
		std::vector<ShieldGpu> m_Shields;
		std::vector<CellGpu> m_Cells;
	};
}

#pragma once
#include "UI/UiTypes.h"
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace Game
{
	enum class BackdropMood : uint8_t
	{
		Calm,
		Kindle,
		Seek,
		Wanderer,
		Attune,
		Wardrobe,
		Count,
	};

	struct CosmosStation
	{
		float Yaw = 0.0f;
		float Pitch = 0.4f;
		float Distance = 2.3f;
		glm::vec3 Target{ 0.0f };
		float Roll = 0.0f;
		glm::vec2 Frame{ 0.0f };
		glm::vec3 Tint{ 1.0f };
		float Exposure = 1.0f;
	};

	inline constexpr CosmosStation CosmosStations[] = {
		{ .Yaw = 0.55f, .Pitch = 0.5f, .Distance = 2.35f, .Target = { 0.0f, 0.0f, 0.0f }, .Roll = -0.22f, .Frame = { 0.3f, 0.04f }, .Tint = { 1.0f, 0.97f, 1.0f }, .Exposure = 1.0f },
		{ .Yaw = 0.95f, .Pitch = 0.42f, .Distance = 2.35f, .Target = { 0.0f, 0.0f, 0.0f }, .Roll = -0.14f, .Frame = { 0.56f, 0.03f }, .Tint = { 1.06f, 0.95f, 0.88f }, .Exposure = 1.0f },
		{ .Yaw = 1.35f, .Pitch = 0.34f, .Distance = 2.4f, .Target = { 0.0f, 0.0f, 0.0f }, .Roll = -0.06f, .Frame = { 0.58f, 0.02f }, .Tint = { 0.9f, 1.0f, 1.08f }, .Exposure = 1.04f },
		{ .Yaw = 1.75f, .Pitch = 0.6f, .Distance = 2.3f, .Target = { 0.0f, 0.0f, 0.0f }, .Roll = 0.06f, .Frame = { 0.55f, 0.04f }, .Tint = { 1.04f, 0.92f, 1.06f }, .Exposure = 1.04f },
		{ .Yaw = 2.55f, .Pitch = 0.72f, .Distance = 2.4f, .Target = { 0.0f, 0.0f, 0.0f }, .Roll = 0.0f, .Frame = { 0.6f, 0.02f }, .Tint = { 0.94f, 0.96f, 1.06f }, .Exposure = 1.02f },
		{ .Yaw = 2.15f, .Pitch = 0.45f, .Distance = 2.35f, .Target = { 0.0f, 0.0f, 0.0f }, .Roll = 0.1f, .Frame = { 0.57f, 0.03f }, .Tint = { 1.04f, 0.94f, 1.0f }, .Exposure = 1.02f },
	};
	static_assert(std::size(CosmosStations) == size_t(BackdropMood::Count));

	struct BackdropView
	{
		float Time = 0.0f;
		BackdropMood Mood = BackdropMood::Calm;
		glm::vec2 Mouse{ -1e4f };
		glm::vec2 Parallax{ 0.0f };
		float Fade = 1.0f;
		float Dive = 0.0f;
		float UiScale = 1.0f;
		std::span<const UiPrimitive> Title;
		float TitleTime = 0.0f;
		float TitleShown = 1.0f;
		glm::vec4 Cloud{ 0.0f };
		float PageShown = 0.0f;
		glm::vec2 Orbit{ 0.0f };
		float Tour = 0.0f;
	};

	class MenuBackdrop
	{
	public:
		MenuBackdrop(Rdn::Device& device, Rdn::ResourceAllocator& allocator, Rdn::Format targetFormat, const Rdn::GpuImage* atlas);
		~MenuBackdrop();
		MenuBackdrop(const MenuBackdrop&) = delete;
		MenuBackdrop& operator=(const MenuBackdrop&) = delete;

		bool Render(Rdn::RenderGraph& graph, Rdn::RGImage target, Rdn::Extent2D extent, uint32_t frameIndex, const BackdropView& view);
		void RenderVeil(Rdn::RenderGraph& graph, Rdn::Extent2D extent, float time);
		Rdn::RGImage TakeLight() { return std::exchange(m_Light, Rdn::RGImage{}); }

	private:
		static constexpr int WardenBoneCount = 7;

		struct Frame
		{
			glm::mat4 ViewProjection;
			glm::mat4 PreviousViewProjection;
			glm::vec4 Camera;
			glm::vec4 Right;
			glm::vec4 Up;
			glm::vec4 Forward;
			glm::vec4 Screen;
			glm::vec4 Lens;
			glm::vec4 Mouse;
			glm::vec4 Tint;
			glm::vec4 Galaxy;
			glm::vec4 Title;
			glm::vec4 Counts;
			glm::vec4 Cloud;
			glm::vec4 CloudState;
			glm::vec4 Warden[11];
			glm::vec4 WardenBones[WardenBoneCount * 3];
		};

		struct Glyph
		{
			glm::vec4 Rect;
			glm::vec4 Uv;
			glm::vec4 Area;
		};

		struct Camera
		{
			float Yaw = 0.0f;
			float Pitch = 0.0f;
			float Distance = 1.0f;
			glm::vec3 Target{ 0.0f };
			float Roll = 0.0f;
			glm::vec2 Frame{ 0.0f };
			glm::vec3 Tint{ 1.0f };
			float Exposure = 1.0f;
		};

		static constexpr uint32_t MaxGlyphs = 48;
		static constexpr Rdn::Format HdrFormat = Rdn::Format::R16G16B16A16_Sfloat;
		static constexpr Rdn::Format LightFormat = Rdn::Format::R8G8B8A8_Unorm;

		Rdn::Pipeline* CreatePipeline(const char* vertex, const char* fragment, Rdn::Format format, Rdn::BlendMode blend, Rdn::ColorComponent mask = Rdn::ColorComponent::All);
		Camera Blend(const Camera& from, const Camera& to, float t) const;
		void UpdateCamera(const BackdropView& view, float dt);
		void LoadWarden();
		void PoseWarden(float time, float tour, float lean, glm::vec4* rows, glm::vec4* curls) const;

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		Rdn::Format m_Format;
		const Rdn::GpuImage* m_Atlas = nullptr;
		std::vector<std::unique_ptr<Rdn::Shader>> m_Shaders;
		std::vector<std::unique_ptr<Rdn::Pipeline>> m_Pipelines;
		Rdn::Pipeline* m_Bake = nullptr;
		Rdn::Pipeline* m_Sky = nullptr;
		Rdn::Pipeline* m_Points = nullptr;
		Rdn::Pipeline* m_Composite = nullptr;
		Rdn::Pipeline* m_LightPass = nullptr;
		Rdn::Pipeline* m_Bloom = nullptr;
		Rdn::Pipeline* m_Veil = nullptr;
		std::unique_ptr<Rdn::GpuImage> m_Nebula;
		std::unique_ptr<Rdn::Sampler> m_Sampler;
		std::unique_ptr<Rdn::Sampler> m_ClampSampler;
		std::unique_ptr<Rdn::GpuRingBuffer> m_Buffer;
		std::unique_ptr<Rdn::GpuBuffer> m_Warden;
		uint32_t m_NebulaSize = 2048;
		uint32_t m_BakeBands = 1;
		uint32_t m_BakedBands = 0;
		uint32_t m_GalaxyStars = 200000;
		uint32_t m_DustMotes = 3000;
		uint32_t m_TitleMotes = 260;
		uint32_t m_WardenStars = 320000;
		uint32_t m_WardenCount = 0;
		glm::vec3 m_WardenLow{ 0.0f };
		glm::vec3 m_WardenExtent{ 1.0f };
		glm::vec3 m_WardenEyes[2]{};
		glm::vec3 m_WardenJoints[2][3]{};
		struct WardenHand
		{
			glm::vec3 Wrist{ 0.0f };
			glm::vec3 Along{ 0.0f, -1.0f, 0.0f };
			glm::vec3 Palm{ 1.0f, 0.0f, 0.0f };
			float Knuckle = 0.0f;
			float Middle = 0.0f;
		};
		WardenHand m_WardenHands[2]{};
		float m_Scale = 1.0f;
		float m_Quality = 1.0f;
		bool m_Ready = false;

		Camera m_From;
		Camera m_Shown;
		BackdropMood m_Mood = BackdropMood::Calm;
		float m_Flight = 1.0f;
		float m_LastTime = -1.0f;
		glm::vec2 m_Mouse{ -1e4f };
		float m_Stir = 0.0f;
		glm::mat4 m_PreviousViewProjection{ 1.0f };
		bool m_HasPrevious = false;
		Rdn::RGImage m_Light;
	};
}

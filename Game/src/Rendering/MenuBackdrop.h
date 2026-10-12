#pragma once
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <iterator>
#include <memory>

namespace Game
{
	enum class BackdropMood : uint8_t
	{
		Calm,
		Kindle,
		Seek,
		Wanderer,
		Attune,
		Count,
	};

	struct BackdropView
	{
		float Time = 0.0f;
		float Focus = 0.0f;
		glm::vec2 Mouse{ 0.0f };
		float Fade = 1.0f;
		float Angle = 0.0f;
		glm::vec3 Tint{ 0.58f, 0.42f, 1.0f };
		float Fracture = 1.0f;
		float Sides = 8.0f;
		float Meet = 3.0f;
	};

	inline constexpr BackdropView BackdropMoods[] = {
		{ .Angle = 0.0f, .Tint = { 0.58f, 0.42f, 1.0f }, .Fracture = 1.0f, .Sides = 8.0f, .Meet = 3.0f },
		{ .Angle = 0.5f, .Tint = { 1.0f, 0.62f, 0.32f }, .Fracture = 1.3f, .Sides = 6.0f, .Meet = 4.0f },
		{ .Angle = 1.0f, .Tint = { 0.34f, 0.84f, 1.0f }, .Fracture = 1.3f, .Sides = 4.0f, .Meet = 6.0f },
		{ .Angle = 1.6f, .Tint = { 0.95f, 0.44f, 0.82f }, .Fracture = 1.3f, .Sides = 10.0f, .Meet = 3.0f },
		{ .Angle = 2.2f, .Tint = { 0.6f, 0.68f, 1.0f }, .Fracture = 1.15f, .Sides = 8.0f, .Meet = 4.0f },
	};
	static_assert(std::size(BackdropMoods) == size_t(BackdropMood::Count));

	class MenuBackdrop
	{
	public:
		MenuBackdrop(Rdn::Device& device, Rdn::ResourceAllocator& allocator, Rdn::Format targetFormat);
		~MenuBackdrop();
		MenuBackdrop(const MenuBackdrop&) = delete;
		MenuBackdrop& operator=(const MenuBackdrop&) = delete;

		bool Render(Rdn::RenderGraph& graph, Rdn::RGImage target, Rdn::Extent2D extent, const BackdropView& view);

	private:
		struct Constants
		{
			glm::vec2 Resolution;
			glm::vec2 Mouse;
			glm::vec4 Tint;
			float Time;
			float Focus;
			float Fade;
			float Quality;
			float Angle;
			float Fracture;
			float Pulse;
			float Sides;
			float Meet;
			float Padding[3];
		};

		Rdn::Device& m_Device;
		Rdn::ResourceAllocator& m_Allocator;
		Rdn::Format m_Format;
		std::unique_ptr<Rdn::Shader> m_VertexShader;
		std::unique_ptr<Rdn::Shader> m_FragmentShader;
		std::unique_ptr<Rdn::Pipeline> m_Pipeline;
		float m_Scale = 1.0f;
		float m_Quality = 1.0f;
		BackdropView m_Shown;
		float m_Target = 0.0f;
		float m_PulseStart = 0.0f;
		float m_LastTime = -1.0f;
	};
}

#pragma once
#include "Rendering/Lighting.h"
#include <RadianceEngine/Graphics/Device.h>
#include <cstdint>
#include <iterator>

namespace Game
{
	enum class GraphicsTier : uint8_t
	{
		Low,
		Medium,
		High,
		Count,
	};

	struct GraphicsPreset
	{
		ShadowQuality Shadows = ShadowQuality::High;
		GiQuality Gi = GiQuality::Medium;
		uint32_t FrameCap = 240;
	};

	inline constexpr uint32_t MinFrameCap = 30;
	inline constexpr uint32_t MaxFrameCap = 240;

	inline constexpr const char* GraphicsTierNames[] = { "low", "medium", "high" };
	inline constexpr GraphicsPreset GraphicsPresets[] = {
		{ ShadowQuality::Low, GiQuality::Off, 60 },
		{ ShadowQuality::High, GiQuality::Low, 144 },
		{ ShadowQuality::High, GiQuality::Medium, 240 },
	};
	static_assert(std::size(GraphicsTierNames) == size_t(GraphicsTier::Count) && std::size(GraphicsPresets) == size_t(GraphicsTier::Count));

	inline GraphicsTier RecommendGraphics(const Rdn::DeviceProperties& device)
	{
		constexpr uint64_t GiB = 1ull << 30;
		if (!device.Discrete || device.DeviceLocalMemory < 3 * GiB)
			return GraphicsTier::Low;
		if (device.DeviceLocalMemory < 5 * GiB + GiB / 2)
			return GraphicsTier::Medium;
		return GraphicsTier::High;
	}
}

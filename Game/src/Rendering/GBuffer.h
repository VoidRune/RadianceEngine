#pragma once
#include <RadianceEngine/Graphics/RenderGraph.h>

namespace Game
{
	inline constexpr Rdn::Format SceneColorFormat = Rdn::Format::R16G16B16A16_Sfloat;
	inline constexpr Rdn::Format AlbedoFormat = Rdn::Format::R8G8B8A8_Srgb;
	inline constexpr Rdn::Format SurfaceFormat = Rdn::Format::A2B10G10R10_Unorm;
	inline constexpr Rdn::Format EmissionFormat = Rdn::Format::R16G16B16A16_Sfloat;
	inline constexpr Rdn::Format DepthFormat = Rdn::Format::D32_Sfloat;

	struct GBuffer
	{
		Rdn::RGImage Albedo;
		Rdn::RGImage Surface;
		Rdn::RGImage Emission;
		Rdn::RGImage Depth;
	};
}

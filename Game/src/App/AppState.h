#pragma once
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <cstdint>

namespace Game
{
	class AppState
	{
	public:
		virtual ~AppState() = default;

		virtual void Update(float) {}
		virtual void DrawUi() {}
		virtual bool Render(Rdn::RenderGraph&, Rdn::RGImage, Rdn::Extent2D, uint32_t) { return false; }
		virtual bool WantsVsync() const { return false; }
	};
}

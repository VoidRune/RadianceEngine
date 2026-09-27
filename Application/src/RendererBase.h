#pragma once
#include <string>

class RendererBase
{
public:
	RendererBase() {}
	virtual ~RendererBase() {}
	virtual void RenderFrame(float elapsedTime) {}
	virtual void SwapchainResized() {}
	virtual void RecompileShaders() {}
	virtual void WaitForFrameEnd() {}
	virtual std::string GetStatus() const { return {}; }
};
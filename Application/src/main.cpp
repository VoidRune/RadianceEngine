#include <RadianceEngine/Window/Window.h>
#include <RadianceEngine/Window/Input.h>
#include <RadianceEngine/Graphics/Device.h>
#include <RadianceEngine/Graphics/PresentQueue.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Core/Timer.h>
#include "PathTracer/PathTracer.h"

int currentRendererId = -1;
void GetRenderer(int rendererId, std::unique_ptr<RendererBase>& renderer,
	Rdn::Window* window, Rdn::Device* device, Rdn::PresentQueue* presentQueue, Rdn::ResourceAllocator* resourceAllocator, Rdn::RenderGraph* renderGraph)
{
	if (currentRendererId == rendererId)
		return;

	device->WaitIdle();
	renderGraph->ReleaseTransientResources();
	resourceAllocator->FreeResources();
	renderer.reset();
	switch (rendererId)
	{
	case 1:
		renderer = std::make_unique<PathTracer>(window, device, presentQueue, resourceAllocator, renderGraph);
		break;
	}
	currentRendererId = rendererId;
}


void main()
{
	Rdn::WindowDescription windowDesc;
	windowDesc.Title = "Arcane Vulkan renderer";
	windowDesc.Width = 1280;
	windowDesc.Height = 720;
	windowDesc.Fullscreen = false;
	auto window = std::make_unique<Rdn::Window>(windowDesc);

	Rdn::DeviceConfig deviceConfig;
	deviceConfig.WindowHandle = window->GetHandle();
	deviceConfig.InstanceExtensions = window->GetInstanceExtensions();
	deviceConfig.FramesInFlight = 2;
	deviceConfig.EnableValidation = true;
	auto device = std::make_unique<Rdn::Device>(deviceConfig);
	Rdn::PresentMode presentMode = Rdn::PresentMode::Mailbox;
	auto presentQueue = std::make_unique<Rdn::PresentQueue>(device.get(), presentMode);

	auto resourceAllocator = std::make_unique<Rdn::ResourceAllocator>(device.get());
	auto renderGraph = std::make_unique<Rdn::RenderGraph>(device.get(), resourceAllocator.get());

	std::unique_ptr<RendererBase> renderer;
	GetRenderer(1, renderer, window.get(), device.get(), presentQueue.get(), resourceAllocator.get(), renderGraph.get());

	Rdn::Timer timer;
	while (!window->IsClosed())
	{
		window->PollEvents();

		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::Escape))
			window->SetClosed(true);
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F1))
			window->SetFullscreen(!window->IsFullscreen());
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::V))
			presentQueue->SetPresentMode(presentQueue->GetPresentMode() == Rdn::PresentMode::Fifo ? Rdn::PresentMode::Mailbox : Rdn::PresentMode::Fifo);
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F5))
			renderer->RecompileShaders();

		if (presentQueue->NeedsRecreate())
		{
			const Rdn::Extent2D previousExtent = presentQueue->GetExtent();
			if (!presentQueue->Recreate())
			{
				window->WaitEvents();
				continue;
			}
			if (presentQueue->GetExtent() != previousExtent)
				renderer->SwapchainResized();
		}

		renderer->RenderFrame(float(timer.ElapsedSeconds()));
	}

	device->WaitIdle();
	renderGraph->ReleaseTransientResources();
	resourceAllocator->FreeResources();
}
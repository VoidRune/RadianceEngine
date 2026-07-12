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
	auto renderGraph = std::make_unique<Rdn::RenderGraph>(device.get());

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

		if (presentQueue->OutOfDate())
		{
			while (window->Width() == 0 || window->Height() == 0)
				window->WaitEvents();
			device->WaitIdle();
			presentQueue.reset();
			presentQueue = std::make_unique<Rdn::PresentQueue>(device.get(), presentMode);
			renderer->SwapchainResized(presentQueue.get());
		}

		renderer->RenderFrame(timer.elapsed_sec());
	}

	device->WaitIdle();
	resourceAllocator->FreeResources();
}
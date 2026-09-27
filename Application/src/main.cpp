#include <RadianceEngine/Window/Window.h>
#include <RadianceEngine/Window/Input.h>
#include <RadianceEngine/Graphics/Device.h>
#include <RadianceEngine/Graphics/PresentQueue.h>
#include <RadianceEngine/Graphics/ResourceAllocator.h>
#include <RadianceEngine/Graphics/RenderGraph.h>
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Core/Timer.h>
#include "PathTracer/PathTracer.h"
#include <charconv>
#include <format>
#include <string_view>
#include <vector>

namespace
{
	bool ParseFloat(std::string_view text, float& value)
	{
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
		return error == std::errc() && end == text.data() + text.size();
	}

	bool ParseUint(std::string_view text, uint32_t& value)
	{
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
		return error == std::errc() && end == text.data() + text.size();
	}

	bool ParseFloats(std::string_view text, std::vector<float>& values)
	{
		values.clear();
		while (true)
		{
			const size_t comma = text.find(',');
			float value;
			if (!ParseFloat(text.substr(0, comma), value))
				return false;
			values.push_back(value);
			if (comma == std::string_view::npos)
				return true;
			text.remove_prefix(comma + 1);
		}
	}

	PathTracerOptions ParseOptions(int argc, char** argv, Rdn::WindowDescription& windowDesc)
	{
		constexpr std::string_view usage = "usage: Application [scene.gltf|scene.glb|model.obj] [--env sky.hdr] [--env-intensity x] [--env-rotation degrees] "
			"[--exposure x] [--clamp x] [--filter sigma] [--camera x,y,z,yaw,pitch[,fov]] [--screenshot file.png --frames n] [--size WxH] [--fullscreen] [--no-validation]";
		PathTracerOptions options;
		for (int i = 1; i < argc; i++)
		{
			const std::string_view argument = argv[i];
			const bool takesValue = argument == "--env" || argument == "--env-intensity" || argument == "--env-rotation" || argument == "--exposure"
				|| argument == "--clamp" || argument == "--filter" || argument == "--camera" || argument == "--screenshot" || argument == "--frames"
				|| argument == "--size";
			if (takesValue && i + 1 >= argc)
			{
				RDN_LOG_ERROR("{} needs a value ({})", argument, usage);
				break;
			}
			const std::string_view value = takesValue ? std::string_view(argv[++i]) : std::string_view();

			bool valid = true;
			std::vector<float> values;
			if (argument == "--env")
				options.EnvironmentPath = value;
			else if (argument == "--env-intensity")
				valid = ParseFloat(value, options.EnvironmentIntensity);
			else if (argument == "--env-rotation")
				valid = ParseFloat(value, options.EnvironmentRotation);
			else if (argument == "--exposure")
				valid = ParseFloat(value, options.Exposure);
			else if (argument == "--clamp")
				valid = ParseFloat(value, options.IndirectClamp);
			else if (argument == "--filter")
				valid = ParseFloat(value, options.FilterSigma);
			else if (argument == "--camera")
			{
				valid = ParseFloats(value, values) && (values.size() == 5 || values.size() == 6);
				if (valid)
					options.Camera = CameraPose{ { values[0], values[1], values[2] }, values[3], values[4], values.size() == 6 ? values[5] : 50.0f };
			}
			else if (argument == "--screenshot")
				options.ScreenshotPath = value;
			else if (argument == "--frames")
				valid = ParseUint(value, options.ScreenshotFrames);
			else if (argument == "--size")
			{
				const size_t separator = value.find('x');
				uint32_t width = 0;
				uint32_t height = 0;
				valid = separator != std::string_view::npos && ParseUint(value.substr(0, separator), width) && ParseUint(value.substr(separator + 1), height)
					&& width > 0 && height > 0;
				if (valid)
				{
					windowDesc.Width = width;
					windowDesc.Height = height;
				}
			}
			else if (argument == "--fullscreen")
				windowDesc.Fullscreen = true;
			else if (argument == "--no-validation")
				options.Validation = false;
			else if (argument.starts_with("--"))
				valid = false;
			else
				options.ScenePath = argument;

			if (!valid)
				RDN_LOG_ERROR("Ignoring the invalid argument {} {} ({})", argument, value, usage);
		}
		return options;
	}
}

int currentRendererId = -1;
void GetRenderer(int rendererId, std::unique_ptr<RendererBase>& renderer, const PathTracerOptions& options,
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
		renderer = std::make_unique<PathTracer>(window, device, presentQueue, resourceAllocator, renderGraph, options);
		break;
	}
	currentRendererId = rendererId;
}


int main(int argc, char** argv)
{
	Rdn::WindowDescription windowDesc;
	windowDesc.Title = "Arcane Vulkan renderer";
	windowDesc.Width = 1280;
	windowDesc.Height = 720;
	windowDesc.Fullscreen = false;
	const PathTracerOptions options = ParseOptions(argc, argv, windowDesc);
	auto window = std::make_unique<Rdn::Window>(windowDesc);

	Rdn::DeviceConfig deviceConfig;
	deviceConfig.WindowHandle = window->GetHandle();
	deviceConfig.InstanceExtensions = window->GetInstanceExtensions();
	deviceConfig.FramesInFlight = 2;
	deviceConfig.EnableValidation = options.Validation;
	auto device = std::make_unique<Rdn::Device>(deviceConfig);
	Rdn::PresentMode presentMode = Rdn::PresentMode::Mailbox;
	auto presentQueue = std::make_unique<Rdn::PresentQueue>(device.get(), presentMode);

	auto resourceAllocator = std::make_unique<Rdn::ResourceAllocator>(device.get());
	auto renderGraph = std::make_unique<Rdn::RenderGraph>(device.get(), resourceAllocator.get());

	std::unique_ptr<RendererBase> renderer;
	GetRenderer(1, renderer, options, window.get(), device.get(), presentQueue.get(), resourceAllocator.get(), renderGraph.get());

	Rdn::Timer timer;
	double nextTitleTime = 0.0;
	double renderSeconds = 0.0;
	uint32_t renderedFrames = 0;
	while (!window->IsClosed())
	{
		window->PollEvents();
		if (window->IsMinimized())
		{
			window->WaitEvents();
			continue;
		}

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

		const double frameStart = timer.ElapsedSeconds();
		renderer->RenderFrame(float(frameStart));
		renderSeconds += timer.ElapsedSeconds() - frameStart;
		renderedFrames++;
		if (frameStart >= nextTitleTime)
		{
			window->SetTitle(std::format("{} | {} | {:.2f} ms", windowDesc.Title, renderer->GetStatus(), 1000.0 * renderSeconds / renderedFrames));
			nextTitleTime = frameStart + 0.25;
			renderSeconds = 0.0;
			renderedFrames = 0;
		}
	}

	device->WaitIdle();
	renderGraph->ReleaseTransientResources();
	resourceAllocator->FreeResources();
	return 0;
}

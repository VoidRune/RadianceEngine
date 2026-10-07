#include "ImGuiLayer.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Graphics/ShaderCompiler.h>
#include <RadianceEngine/Window/Input.h>
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <filesystem>

namespace Game
{
	namespace
	{
		constexpr const char* VertexSource = R"(#version 450
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec4 inColor;
layout(push_constant) uniform PushConstants { vec2 Scale; vec2 Translate; } pc;
layout(location = 0) out vec2 outUv;
layout(location = 1) out vec4 outColor;
void main()
{
	outUv = inUv;
	outColor = inColor;
	gl_Position = vec4(inPosition * pc.Scale + pc.Translate, 0.0, 1.0);
}
)";

		constexpr const char* FragmentSource = R"(#version 450
layout(set = 0, binding = 0) uniform sampler2D fontTexture;
layout(location = 0) in vec2 inUv;
layout(location = 1) in vec4 inColor;
layout(location = 0) out vec4 outColor;
void main()
{
	outColor = inColor * texture(fontTexture, inUv);
}
)";

		struct KeyMapping
		{
			Rdn::KeyCode Key;
			ImGuiKey ImGuiKey;
		};

		constexpr KeyMapping Keys[] = {
			{ Rdn::KeyCode::Tab, ImGuiKey_Tab }, { Rdn::KeyCode::Left, ImGuiKey_LeftArrow }, { Rdn::KeyCode::Right, ImGuiKey_RightArrow },
			{ Rdn::KeyCode::Up, ImGuiKey_UpArrow }, { Rdn::KeyCode::Down, ImGuiKey_DownArrow }, { Rdn::KeyCode::PageUp, ImGuiKey_PageUp },
			{ Rdn::KeyCode::PageDown, ImGuiKey_PageDown }, { Rdn::KeyCode::Home, ImGuiKey_Home }, { Rdn::KeyCode::End, ImGuiKey_End },
			{ Rdn::KeyCode::Insert, ImGuiKey_Insert }, { Rdn::KeyCode::Delete, ImGuiKey_Delete }, { Rdn::KeyCode::Backspace, ImGuiKey_Backspace },
			{ Rdn::KeyCode::Space, ImGuiKey_Space }, { Rdn::KeyCode::Enter, ImGuiKey_Enter }, { Rdn::KeyCode::KpEnter, ImGuiKey_KeypadEnter },
			{ Rdn::KeyCode::Escape, ImGuiKey_Escape }, { Rdn::KeyCode::A, ImGuiKey_A }, { Rdn::KeyCode::C, ImGuiKey_C },
			{ Rdn::KeyCode::V, ImGuiKey_V }, { Rdn::KeyCode::X, ImGuiKey_X }, { Rdn::KeyCode::Y, ImGuiKey_Y }, { Rdn::KeyCode::Z, ImGuiKey_Z },
			{ Rdn::KeyCode::LeftShift, ImGuiKey_LeftShift }, { Rdn::KeyCode::RightShift, ImGuiKey_RightShift },
			{ Rdn::KeyCode::LeftControl, ImGuiKey_LeftCtrl }, { Rdn::KeyCode::RightControl, ImGuiKey_RightCtrl },
			{ Rdn::KeyCode::LeftAlt, ImGuiKey_LeftAlt }, { Rdn::KeyCode::RightAlt, ImGuiKey_RightAlt },
		};

		template<typename Function>
		void ForwardTransitions(Rdn::KeyCode key, Function&& send)
		{
			const bool pressed = Rdn::Input::IsKeyPressed(key);
			const bool released = Rdn::Input::IsKeyReleased(key);
			if (pressed && released)
			{
				const bool down = Rdn::Input::IsKeyDown(key);
				send(!down);
				send(down);
			}
			else if (pressed || released)
				send(pressed);
		}

		uint64_t RoundCapacity(uint64_t bytes)
		{
			constexpr uint64_t granularity = 64 * 1024;
			return std::max(granularity, (bytes * 3 / 2 + granularity - 1) / granularity * granularity);
		}
	}

	ImGuiLayer::ImGuiLayer(Rdn::Device& device, Rdn::ResourceAllocator& allocator, Rdn::Format targetFormat)
		: m_Device(device), m_Allocator(allocator)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = nullptr;
		io.BackendRendererName = "RadianceEngine";
		io.BackendPlatformName = "RadianceEngine";
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 6.0f;
		style.FrameRounding = 4.0f;
		style.FramePadding = ImVec2(8.0f, 5.0f);
		style.ItemSpacing = ImVec2(8.0f, 7.0f);

		CreateFont();
		CreatePipeline(targetFormat);
	}

	ImGuiLayer::~ImGuiLayer()
	{
		m_Device.WaitIdle();
		if (m_VertexBuffer)
			m_Allocator.ReleaseResource(m_VertexBuffer.get());
		if (m_IndexBuffer)
			m_Allocator.ReleaseResource(m_IndexBuffer.get());
		m_Allocator.ReleaseResource(m_Pipeline.get());
		m_Allocator.ReleaseResource(m_VertexShader.get());
		m_Allocator.ReleaseResource(m_FragmentShader.get());
		m_Allocator.ReleaseResource(m_FontImage.get());
		m_Allocator.ReleaseResource(m_Sampler.get());
		ImGui::DestroyContext();
	}

	void ImGuiLayer::CreateFont()
	{
		ImGuiIO& io = ImGui::GetIO();
		const char* uiFont = "C:/Windows/Fonts/segoeui.ttf";
		if (!std::filesystem::exists(uiFont) || !io.Fonts->AddFontFromFileTTF(uiFont, 20.0f))
		{
			ImFontConfig config;
			config.SizePixels = 20.0f;
			io.Fonts->AddFontDefault(&config);
		}

		unsigned char* pixels = nullptr;
		int width = 0;
		int height = 0;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
		m_FontImage = std::make_unique<Rdn::GpuImage>();
		m_Allocator.CreateGpuImage(m_FontImage.get(), Rdn::GpuImageDesc{
			.ImageSize = { uint32_t(width), uint32_t(height), 1 },
			.Format = Rdn::Format::R8G8B8A8_Unorm,
			.UsageFlags = Rdn::ImageUsage::Sampled | Rdn::ImageUsage::TransferDst,
		});
		m_Allocator.SetImageData(m_FontImage.get(), pixels, uint64_t(width) * height * 4, Rdn::ImageLayout::ShaderReadOnlyOptimal);
		io.Fonts->SetTexID(ImTextureID(1));

		m_Sampler = std::make_unique<Rdn::Sampler>();
		m_Allocator.CreateSampler(m_Sampler.get(), Rdn::SamplerDesc{ .AddressMode = Rdn::SamplerAddressMode::ClampToEdge });
	}

	void ImGuiLayer::CreatePipeline(Rdn::Format targetFormat)
	{
		Rdn::ShaderDesc vertexDesc;
		Rdn::ShaderDesc fragmentDesc;
		if (!Rdn::ShaderCompiler::CompileFromSource(VertexSource, Rdn::ShaderStage::Vertex, vertexDesc, "ImGui.vert")
			|| !Rdn::ShaderCompiler::CompileFromSource(FragmentSource, Rdn::ShaderStage::Fragment, fragmentDesc, "ImGui.frag"))
		{
			RDN_LOG_FATAL("Failed to compile the ImGui shaders");
			return;
		}
		m_VertexShader = std::make_unique<Rdn::Shader>();
		m_FragmentShader = std::make_unique<Rdn::Shader>();
		m_Allocator.CreateShader(m_VertexShader.get(), vertexDesc);
		m_Allocator.CreateShader(m_FragmentShader.get(), fragmentDesc);

		m_Pipeline = std::make_unique<Rdn::Pipeline>();
		m_Allocator.CreatePipeline(m_Pipeline.get(), Rdn::PipelineDesc{
			.ShaderStages = { m_VertexShader.get(), m_FragmentShader.get() },
			.CullMode = Rdn::CullMode::None,
			.DepthTest = false,
			.DepthWrite = false,
			.AlphaBlend = true,
			.VertexAttributes = Rdn::VertexAttributes({
				{ Rdn::Format::R32G32_Sfloat, offsetof(ImDrawVert, pos) },
				{ Rdn::Format::R32G32_Sfloat, offsetof(ImDrawVert, uv) },
				{ Rdn::Format::R8G8B8A8_Unorm, offsetof(ImDrawVert, col) },
			}, sizeof(ImDrawVert)),
			.ColorAttachmentFormats = { targetFormat },
			.PushDescriptorSets = { 0 },
		});
	}

	void ImGuiLayer::BeginFrame(float dt, Rdn::Extent2D displaySize)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2(float(displaySize.Width), float(displaySize.Height));
		io.DeltaTime = std::max(dt, 1e-4f);
		ForwardInput();
		ImGui::NewFrame();
	}

	void ImGuiLayer::EndFrame()
	{
		ImGui::Render();
	}

	void ImGuiLayer::ForwardInput()
	{
		ImGuiIO& io = ImGui::GetIO();
		io.AddMousePosEvent(Rdn::Input::GetMouseX(), Rdn::Input::GetMouseY());
		const Rdn::KeyCode buttons[] = { Rdn::KeyCode::MouseLeft, Rdn::KeyCode::MouseRight, Rdn::KeyCode::MouseMiddle };
		for (int i = 0; i < 3; i++)
			ForwardTransitions(buttons[i], [&](bool down) { io.AddMouseButtonEvent(i, down); });
		if (Rdn::Input::GetScrollVertical() != 0.0f || Rdn::Input::GetScrollHorizontal() != 0.0f)
			io.AddMouseWheelEvent(Rdn::Input::GetScrollHorizontal(), Rdn::Input::GetScrollVertical());

		io.AddKeyEvent(ImGuiMod_Ctrl, Rdn::Input::IsKeyDown(Rdn::KeyCode::LeftControl) || Rdn::Input::IsKeyDown(Rdn::KeyCode::RightControl));
		io.AddKeyEvent(ImGuiMod_Shift, Rdn::Input::IsKeyDown(Rdn::KeyCode::LeftShift) || Rdn::Input::IsKeyDown(Rdn::KeyCode::RightShift));
		io.AddKeyEvent(ImGuiMod_Alt, Rdn::Input::IsKeyDown(Rdn::KeyCode::LeftAlt) || Rdn::Input::IsKeyDown(Rdn::KeyCode::RightAlt));
		for (const KeyMapping& mapping : Keys)
			ForwardTransitions(mapping.Key, [&](bool down) { io.AddKeyEvent(mapping.ImGuiKey, down); });
		for (uint32_t character : Rdn::Input::GetTypedCharacters())
			io.AddInputCharacter(character);
	}

	bool ImGuiLayer::WantsKeyboard() const
	{
		return ImGui::GetIO().WantCaptureKeyboard;
	}

	bool ImGuiLayer::WantsMouse() const
	{
		return ImGui::GetIO().WantCaptureMouse;
	}

	void ImGuiLayer::EnsureCapacity(uint64_t vertexBytes, uint64_t indexBytes)
	{
		if (vertexBytes <= m_VertexCapacity && indexBytes <= m_IndexCapacity)
			return;
		m_Device.WaitIdle();
		const auto recreate = [&](std::unique_ptr<Rdn::GpuRingBuffer>& buffer, uint64_t& capacity, uint64_t bytes, Rdn::BufferUsage usage)
		{
			if (bytes <= capacity)
				return;
			if (buffer)
				m_Allocator.ReleaseResource(buffer.get());
			capacity = RoundCapacity(bytes);
			buffer = std::make_unique<Rdn::GpuRingBuffer>();
			m_Allocator.CreateGpuRingBuffer(buffer.get(), Rdn::GpuRingBufferDesc{
				.ElementSize = uint32_t(capacity),
				.ElementCount = m_Device.GetFramesInFlightCount(),
				.UsageFlags = usage,
				.MemoryProperty = Rdn::MemoryProperty::HostVisible | Rdn::MemoryProperty::HostCoherent,
				.Mapped = true,
			});
		};
		recreate(m_VertexBuffer, m_VertexCapacity, vertexBytes, Rdn::BufferUsage::VertexBuffer);
		recreate(m_IndexBuffer, m_IndexCapacity, indexBytes, Rdn::BufferUsage::IndexBuffer);
	}

	void ImGuiLayer::Render(Rdn::RenderGraph& graph, Rdn::RGImage target, uint32_t frameIndex, bool clear)
	{
		const ImDrawData* data = ImGui::GetDrawData();
		const bool hasGeometry = data != nullptr && data->TotalVtxCount > 0;
		if (!hasGeometry && !clear)
			return;

		if (hasGeometry)
		{
			EnsureCapacity(uint64_t(data->TotalVtxCount) * sizeof(ImDrawVert), uint64_t(data->TotalIdxCount) * sizeof(ImDrawIdx));
			uint8_t* vertices = static_cast<uint8_t*>(m_VertexBuffer->GetMappedPtr()) + m_VertexBuffer->GetOffset(frameIndex);
			uint8_t* indices = static_cast<uint8_t*>(m_IndexBuffer->GetMappedPtr()) + m_IndexBuffer->GetOffset(frameIndex);
			for (const ImDrawList* list : data->CmdLists)
			{
				std::memcpy(vertices, list->VtxBuffer.Data, size_t(list->VtxBuffer.Size) * sizeof(ImDrawVert));
				std::memcpy(indices, list->IdxBuffer.Data, size_t(list->IdxBuffer.Size) * sizeof(ImDrawIdx));
				vertices += size_t(list->VtxBuffer.Size) * sizeof(ImDrawVert);
				indices += size_t(list->IdxBuffer.Size) * sizeof(ImDrawIdx);
			}
		}

		graph.AddPass("ImGui", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(target, clear ? Rdn::AttachmentLoadOp::Clear : Rdn::AttachmentLoadOp::Load, Rdn::AttachmentStoreOp::Store, { 0.035f, 0.04f, 0.055f, 1.0f });
			},
			[this, data, hasGeometry, frameIndex](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				if (!hasGeometry)
					return;
				cmd.BindPipeline(m_Pipeline->GetHandle());
				cmd.PushDescriptorSets(Rdn::PipelineBindPoint::Graphics, m_Pipeline->GetLayout(), 0, Rdn::DescriptorWrite()
					.AddWrite(Rdn::ImageWrite(0, Rdn::DescriptorType::CombinedImageSampler, m_FontImage->GetImageView(), Rdn::ImageLayout::ShaderReadOnlyOptimal, m_Sampler->GetHandle())));
				cmd.BindVertexBuffer(0, m_VertexBuffer->GetHandle(), m_VertexBuffer->GetOffset(frameIndex));
				cmd.BindIndexBuffer(m_IndexBuffer->GetHandle(), sizeof(ImDrawIdx) == 2 ? Rdn::IndexType::Uint16 : Rdn::IndexType::Uint32, m_IndexBuffer->GetOffset(frameIndex));

				const float scale[2] = { 2.0f / data->DisplaySize.x, 2.0f / data->DisplaySize.y };
				const float transform[4] = { scale[0], scale[1], -1.0f - data->DisplayPos.x * scale[0], -1.0f - data->DisplayPos.y * scale[1] };
				cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_Pipeline->GetLayout(), transform, sizeof(transform));

				const float width = float(context.RenderArea.Width);
				const float height = float(context.RenderArea.Height);
				uint32_t vertexOffset = 0;
				uint32_t indexOffset = 0;
				for (const ImDrawList* list : data->CmdLists)
				{
					for (const ImDrawCmd& command : list->CmdBuffer)
					{
						if (command.UserCallback != nullptr)
							continue;
						const float minX = std::clamp((command.ClipRect.x - data->DisplayPos.x) * data->FramebufferScale.x, 0.0f, width);
						const float minY = std::clamp((command.ClipRect.y - data->DisplayPos.y) * data->FramebufferScale.y, 0.0f, height);
						const float maxX = std::clamp((command.ClipRect.z - data->DisplayPos.x) * data->FramebufferScale.x, 0.0f, width);
						const float maxY = std::clamp((command.ClipRect.w - data->DisplayPos.y) * data->FramebufferScale.y, 0.0f, height);
						if (maxX <= minX || maxY <= minY)
							continue;
						cmd.SetScissor(int32_t(minX), int32_t(minY), uint32_t(maxX - minX), uint32_t(maxY - minY));
						cmd.DrawIndexed(command.ElemCount, 1, command.IdxOffset + indexOffset, int32_t(command.VtxOffset + vertexOffset), 0);
					}
					indexOffset += uint32_t(list->IdxBuffer.Size);
					vertexOffset += uint32_t(list->VtxBuffer.Size);
				}
				cmd.SetScissors(context.RenderArea);
			});
	}
}

#include "MenuBackdrop.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Graphics/ShaderCompiler.h>
#include <algorithm>
#include <cmath>

namespace Game
{
	MenuBackdrop::MenuBackdrop(Rdn::Device& device, Rdn::ResourceAllocator& allocator, Rdn::Format targetFormat)
		: m_Device(device), m_Allocator(allocator), m_Format(targetFormat)
	{
		if (device.GetProperties().Integrated)
		{
			m_Scale = 0.75f;
			m_Quality = 0.3f;
		}
		Rdn::ShaderDesc vertexDesc;
		Rdn::ShaderDesc fragmentDesc;
		if (!Rdn::ShaderCompiler::Compile("res/Shaders/Game/Composite.vert", vertexDesc) || !Rdn::ShaderCompiler::Compile("res/Shaders/Game/MenuBackdrop.frag", fragmentDesc))
		{
			RDN_LOG_ERROR("The menu backdrop shaders failed to compile, the menu falls back to a flat background");
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
			.ColorAttachmentFormats = { targetFormat },
		});
	}

	MenuBackdrop::~MenuBackdrop()
	{
		m_Device.WaitIdle();
		if (!m_Pipeline)
			return;
		m_Allocator.ReleaseResource(m_Pipeline.get());
		m_Allocator.ReleaseResource(m_VertexShader.get());
		m_Allocator.ReleaseResource(m_FragmentShader.get());
	}

	bool MenuBackdrop::Render(Rdn::RenderGraph& graph, Rdn::RGImage target, Rdn::Extent2D extent, const BackdropView& view)
	{
		if (!m_Pipeline || extent.Width == 0 || extent.Height == 0)
			return false;
		if (m_LastTime < 0.0f)
		{
			m_Shown = view;
			m_Target = view.Angle;
			m_PulseStart = view.Time;
		}
		if (std::abs(view.Angle - m_Target) > 1e-3f || view.Sides != m_Shown.Sides || view.Meet != m_Shown.Meet)
		{
			m_Target = view.Angle;
			m_PulseStart = view.Time;
			m_Shown.Sides = view.Sides;
			m_Shown.Meet = view.Meet;
		}
		const float ease = 1.0f - std::exp(-std::clamp(view.Time - m_LastTime, 0.0f, 0.1f) * 2.2f);
		m_LastTime = view.Time;
		m_Shown.Focus += (view.Focus - m_Shown.Focus) * ease;
		m_Shown.Angle += std::remainder(view.Angle - m_Shown.Angle, 6.2831853f) * ease;
		m_Shown.Tint += (view.Tint - m_Shown.Tint) * ease;
		m_Shown.Fracture += (view.Fracture - m_Shown.Fracture) * ease;
		const float pulse = view.Time - m_PulseStart;
		const bool scaled = m_Scale < 0.999f;
		const Rdn::Extent2D size = scaled ? Rdn::Extent2D{ std::max(uint32_t(float(extent.Width) * m_Scale), 1u), std::max(uint32_t(float(extent.Height) * m_Scale), 1u) } : extent;
		const Rdn::RGImage canvas = scaled ? graph.CreateImage("Menu backdrop", Rdn::RGImageDesc{ .Size = { size.Width, size.Height, 1 }, .Format = m_Format }) : target;
		const Constants constants{ { float(size.Width), float(size.Height) }, view.Mouse, glm::vec4(m_Shown.Tint, 0.0f), view.Time, m_Shown.Focus, view.Fade, m_Quality, m_Shown.Angle, m_Shown.Fracture + 0.6f * std::exp(-pulse * 2.5f), pulse, m_Shown.Sides, m_Shown.Meet, { 0.0f, 0.0f, 0.0f } };
		graph.AddPass("Menu backdrop", Rdn::PassType::Graphics,
			[&](Rdn::PassBuilder& builder)
			{
				builder.AddColorAttachment(canvas, Rdn::AttachmentLoadOp::DontCare, Rdn::AttachmentStoreOp::Store);
			},
			[this, constants](Rdn::CommandBuffer& cmd, const Rdn::PassContext&)
			{
				cmd.BindPipeline(m_Pipeline->GetHandle());
				cmd.PushConstants(Rdn::ShaderStage::Vertex | Rdn::ShaderStage::Fragment, m_Pipeline->GetLayout(), &constants, sizeof(constants));
				cmd.Draw(3, 1, 0, 0);
			});
		if (!scaled)
			return true;
		graph.AddPass("Menu backdrop upscale", Rdn::PassType::Transfer,
			[&](Rdn::PassBuilder& builder)
			{
				builder.UseImage(canvas, Rdn::RGImageUsage::TransferSrc);
				builder.UseImage(target, Rdn::RGImageUsage::TransferDst);
			},
			[=](Rdn::CommandBuffer& cmd, const Rdn::PassContext& context)
			{
				cmd.BlitImage(context.GetImage(canvas), context.GetImageExtent(canvas), context.GetImage(target), context.GetImageExtent(target), Rdn::Filter::Linear);
			});
		return true;
	}
}

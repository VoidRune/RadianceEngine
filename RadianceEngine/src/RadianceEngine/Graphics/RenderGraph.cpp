#include "RenderGraph.h"

namespace Rdn
{
	RenderGraph::RenderGraph(Device* device)
		: m_Device(device)
		, m_LogicalDevice(device->GetLogicalDevice())
	{

	}

	RenderGraph::~RenderGraph()
	{

	}

    // PASS BUILDER

	void PassBuilder::UseImage(ImageHandle image, ImageLayout layout,
		PipelineStage stage, AccessMask access,
		ImageAspect aspect)
	{
		m_Data->ImageAccesses.push_back({ image, layout, stage, access, aspect });
	}

	void PassBuilder::UseBuffer(BufferHandle buffer, PipelineStage stage, AccessMask access)
	{
		m_Data->BufferAccesses.push_back({ buffer, stage, access });
	}

    void PassBuilder::AddColorAttachment(ImageViewHandle view, ImageHandle backingImage,
        AttachmentLoadOp loadOp, AttachmentStoreOp storeOp,
        float r, float g, float b, float a)
    {
        m_Data->UsesDynamicRendering = true;
        m_Data->ColorAttachments.push_back({
            .ImageView = view, .ImageLayout = ImageLayout::ColorAttachmentOptimal,
            .LoadOp = loadOp, .StoreOp = storeOp, .ClearColor = { r, g, b, a }
            });
        m_Data->ColorAttachmentImages.push_back(backingImage);

        // Color attachment writes happen in the color attachment output stage
        UseImage(backingImage, ImageLayout::ColorAttachmentOptimal,
            PipelineStage::ColorAttachmentOutput,
            (loadOp == AttachmentLoadOp::Load)
            ? (AccessMask::ColorAttachmentRead | AccessMask::ColorAttachmentWrite)
            : AccessMask::ColorAttachmentWrite);
    }

    void PassBuilder::SetDepthAttachment(ImageViewHandle view, ImageHandle backingImage,
        AttachmentLoadOp loadOp, AttachmentStoreOp storeOp,
        float clearDepth)
    {
        m_Data->UsesDynamicRendering = true;
        m_Data->DepthAttach = DepthAttachment{
            .ImageView = view, .ImageLayout = ImageLayout::DepthAttachmentOptimal,
            .LoadOp = loadOp, .StoreOp = storeOp, .ClearDepth = clearDepth
        };
        m_Data->DepthAttachImage = backingImage;

        UseImage(backingImage, ImageLayout::DepthAttachmentOptimal,
            PipelineStage::EarlyFragmentTests | PipelineStage::LateFragmentTests,
            (loadOp == AttachmentLoadOp::Load)
            ? (AccessMask::DepthStencilAttachmentRead | AccessMask::DepthStencilAttachmentWrite)
            : AccessMask::DepthStencilAttachmentWrite,
            ImageAspect::Depth);
    }

    // RENDER GRAPH

    void RenderGraph::AddPass(std::string name, PassSetupFn setup, PassExecuteFn execute)
    {
        PassData pass;
        pass.Name = std::move(name);
        pass.Execute = std::move(execute);

        PassBuilder builder;
        builder.m_Data = &pass;
        setup(builder);

        m_Passes.push_back(std::move(pass));
    }


    void RenderGraph::SetPresentPass(std::string name,
        PassSetupFn setup,
        AttachmentLoadOp loadOp,
        float clearR, float clearG, float clearB, float clearA,
        PassExecuteFn execute)
    {
        PassData pass;
        pass.Name = std::move(name);
        pass.Execute = std::move(execute);
        pass.UsesDynamicRendering = true;


        pass.ColorAttachments.push_back({
            .ImageView = {}, // filled in Execute()
            .ImageLayout = ImageLayout::ColorAttachmentOptimal,
            .LoadOp = loadOp,
            .StoreOp = AttachmentStoreOp::Store,
            .ClearColor = { clearR, clearG, clearB, clearA }
            });
        pass.ColorAttachmentImages.push_back({}); // patched in Execute()

        if (setup)
        {
            PassBuilder builder;
            builder.m_Data = &pass;
            setup(builder);
        }

        m_PresentPass = std::move(pass);
    }

    void RenderGraph::Reset()
    {
        m_Passes.clear();
        m_PresentPass.reset();
    }

    inline constexpr uint32_t WriteAccessMask =
        uint32_t(AccessMask::ShaderWrite)
        | uint32_t(AccessMask::ColorAttachmentWrite)
        | uint32_t(AccessMask::DepthStencilAttachmentWrite)
        | uint32_t(AccessMask::TransferWrite)
        | uint32_t(AccessMask::HostWrite)
        | uint32_t(AccessMask::MemoryWrite);

    inline bool HasWriteAccess(AccessMask mask)
    {
        return (uint32_t(mask) & WriteAccessMask) != 0;
    }

    void RenderGraph::AddBarriersForPass(CommandBuffer& cmd, PassData& pass)
    {
        std::vector<ImageBarrier>  imageBarriers;
        std::vector<BufferBarrier> bufferBarriers;

        for (const ImageAccess& access : pass.ImageAccesses)
        {
            ResourceState& state = m_ImageStates[access.Image.Id];

            bool layoutChanged = state.Layout != access.Layout;
            bool needsSync = layoutChanged || HasWriteAccess(state.Access) || HasWriteAccess(access.Access);

            if (!needsSync && state.Layout == access.Layout)
            {
                continue;
            }

            imageBarriers.push_back(ImageBarrier{
                .Handle = access.Image,
                .OldLayout = state.Layout,
                .NewLayout = access.Layout,
                .SrcStage = state.Stage,
                .DstStage = access.Stage,
                .SrcAccess = state.Access,
                .DstAccess = access.Access,
                .Aspect = access.Aspect,
            });

            state = { access.Layout, access.Stage, access.Access };
        }

        for (const BufferAccess& access : pass.BufferAccesses)
        {
            BufferState& state = m_BufferStates[access.Buffer.Id];

            bool needsSync = HasWriteAccess(state.Access) || HasWriteAccess(access.Access);

            if (needsSync)
            {
                bufferBarriers.push_back(BufferBarrier{
                    .Handle = access.Buffer,
                    .SrcStage = state.Stage,
                    .DstStage = access.Stage,
                    .SrcAccess = state.Access,
                    .DstAccess = access.Access,
                    });
            }

            state = { access.Stage, access.Access };
        }

        if (!imageBarriers.empty() || !bufferBarriers.empty())
            cmd.Barrier(imageBarriers, bufferBarriers);
    }


    void RenderGraph::RecordPass(CommandBuffer& cmd, PassData& pass, uint32_t frameIndex, const Extent2D area)
    {
        AddBarriersForPass(cmd, pass);

        bool hasAttachments = pass.ColorAttachments.size() > 0 || pass.DepthAttach.has_value();
        if (hasAttachments)
            cmd.BeginRendering(pass.ColorAttachments, pass.DepthAttach, area);

        pass.Execute(cmd, frameIndex);

        if (hasAttachments)
            cmd.EndRendering();
    }

    void RenderGraph::Execute(CommandBuffer& cmd, uint32_t frameIndex,
        ImageHandle swapchainImage, ImageViewHandle swapchainView,
        const Extent2D area)
    {
        cmd.SetViewport(area);
        cmd.SetScissors(area);

        for (PassData& pass : m_Passes)
        {
            RecordPass(cmd, pass, frameIndex, area);
        }

        if (m_PresentPass)
        {
            PassData& pass = *m_PresentPass;
            pass.ColorAttachments[0].ImageView = swapchainView;
            pass.ColorAttachmentImages[0] = swapchainImage;

            // The swapchain image's own color-attachment access can't be registered
            // until now, since its handle isn't known until the frame begins (see
            // SetPresentPass). Push it here -- mirroring what AddColorAttachment does
            // for every other pass -- so AddBarriersForPass actually transitions it
            // into ColorAttachmentOptimal before BeginRendering. Previously this was
            // never added at all, so BeginRendering ran against whatever layout the
            // image happened to already be in, and the final transition below started
            // from Undefined (discarding it) instead of the layout it was just
            // rendered in.
            AttachmentLoadOp loadOp = pass.ColorAttachments[0].LoadOp;
            pass.ImageAccesses.push_back(ImageAccess{
                swapchainImage, ImageLayout::ColorAttachmentOptimal,
                PipelineStage::ColorAttachmentOutput,
                (loadOp == AttachmentLoadOp::Load)
                    ? (AccessMask::ColorAttachmentRead | AccessMask::ColorAttachmentWrite)
                    : AccessMask::ColorAttachmentWrite,
                });

            RecordPass(cmd, pass, frameIndex, area);

            cmd.TransitionImage(swapchainImage,
                ImageLayout::ColorAttachmentOptimal,
                ImageLayout::PresentSrc,
                PipelineStage::ColorAttachmentOutput,
                PipelineStage::BottomOfPipe,
                Rdn::ImageAspect::Color);

            m_ImageStates.erase(swapchainImage.Id);
        }
    }
}
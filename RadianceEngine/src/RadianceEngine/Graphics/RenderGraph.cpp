#include "RenderGraph.h"
#include "ResourceAllocator.h"
#include "VulkanInternal/VulkanUtilities.h"
#include "RadianceEngine/Core/Log.h"
#include <vulkan/vk_enum_string_helper.h>

#include <algorithm>
#include <format>

namespace Rdn
{
    namespace
    {
        constexpr AccessMask WriteAccessMask =
            AccessMask::ShaderWrite | AccessMask::ShaderStorageWrite |
            AccessMask::ColorAttachmentWrite | AccessMask::DepthStencilAttachmentWrite |
            AccessMask::TransferWrite | AccessMask::HostWrite | AccessMask::MemoryWrite;

        bool IsWrite(AccessMask access) { return HasAny(access & WriteAccessMask); }
        bool IsRead(AccessMask access) { return HasAny(access & ~WriteAccessMask); }

        ImageAspect AspectOf(Format format)
        {
            switch (format)
            {
            case Format::D16_Unorm:
            case Format::D32_Sfloat:         return ImageAspect::Depth;
            case Format::S8_Uint:            return ImageAspect::Stencil;
            case Format::D16_Unorm_S8_Uint:
            case Format::D24_Unorm_S8_Uint:
            case Format::D32_Sfloat_S8_Uint: return ImageAspect::Depth | ImageAspect::Stencil;
            default:                         return ImageAspect::Color;
            }
        }

        // Layouts an image can only be in while it is bound as a writable attachment.
        bool IsAttachmentLayout(ImageLayout layout)
        {
            return layout == ImageLayout::ColorAttachmentOptimal
                || layout == ImageLayout::DepthStencilAttachmentOptimal
                || layout == ImageLayout::DepthAttachmentOptimal
                || layout == ImageLayout::StencilAttachmentOptimal
                || layout == ImageLayout::AttachmentOptimal;
        }

        PipelineStage ToPipelineStages(ShaderStage stages)
        {
            PipelineStage result = PipelineStage::None;
            if (HasAny(stages & ShaderStage::Vertex))   result |= PipelineStage::VertexShader;
            if (HasAny(stages & ShaderStage::Fragment)) result |= PipelineStage::FragmentShader;
            if (HasAny(stages & ShaderStage::Compute))  result |= PipelineStage::ComputeShader;
            if (HasAny(stages & (ShaderStage::RayGen | ShaderStage::RayAnyHit | ShaderStage::RayClosestHit | ShaderStage::RayMiss)))
                result |= PipelineStage::RayTracingShader;
            return result;
        }

        // Usage flags a transient image must be created with to support an access.
        ImageUsage UsageFor(ImageLayout layout, AccessMask access)
        {
            ImageUsage usage = {};
            if (HasAny(access & (AccessMask::ColorAttachmentRead | AccessMask::ColorAttachmentWrite)))
                usage |= ImageUsage::ColorAttachment;
            if (HasAny(access & (AccessMask::DepthStencilAttachmentRead | AccessMask::DepthStencilAttachmentWrite)))
                usage |= ImageUsage::DepthStencilAttachment;
            if (HasAny(access & AccessMask::InputAttachmentRead))
                usage |= ImageUsage::InputAttachment;
            if (HasAny(access & AccessMask::TransferRead))
                usage |= ImageUsage::TransferSrc;
            if (HasAny(access & AccessMask::TransferWrite))
                usage |= ImageUsage::TransferDst;
            if (HasAny(access & AccessMask::ShaderSampledRead))
                usage |= ImageUsage::Sampled;
            if (HasAny(access & (AccessMask::ShaderStorageRead | AccessMask::ShaderStorageWrite | AccessMask::ShaderWrite)))
                usage |= ImageUsage::Storage;
            if (HasAny(access & AccessMask::ShaderRead)) // legacy flag: storage in General, sampled otherwise
                usage |= (layout == ImageLayout::General) ? ImageUsage::Storage : ImageUsage::Sampled;
            return usage;
        }

        struct SyncOp
        {
            PipelineStage SrcStage = PipelineStage::None;
            AccessMask    SrcAccess = AccessMask::None;
            PipelineStage DstStage = PipelineStage::None;
            AccessMask    DstAccess = AccessMask::None;
            ImageLayout   OldLayout = ImageLayout::Undefined;
            ImageLayout   NewLayout = ImageLayout::Undefined;
        };

        // Advances `state` over one access and reports the barrier that has to precede it, if any.
        //
        // A write or a layout transition waits for the last write and for every read since (WAW/WAR)
        // and makes the last write available. A read in the current layout never waits on other
        // reads, but the last write must have been made visible to its stage and access type.
        // Visibility is tracked as a single (stages x accesses) rectangle, and every visibility
        // barrier covers the whole rectangle, so "visible" is never over-reported.
        bool AdvanceSyncState(ResourceSyncState& state, bool isImage, ImageLayout layout,
            PipelineStage stage, AccessMask access, SyncOp& op)
        {
            const bool write = IsWrite(access);
            const bool transition = isImage && state.Layout != layout;

            if (write || transition)
            {
                op.SrcStage = state.WriteStages | state.ReadStages;
                op.SrcAccess = state.WriteAccess;
                op.DstStage = stage;
                op.DstAccess = access;
                op.OldLayout = state.Layout;
                op.NewLayout = isImage ? layout : state.Layout;
                const bool needed = transition || HasAny(op.SrcStage);

                // For a pure read the layout transition is the write. Its writes are made available
                // automatically, but it still has to be chained from (`stage`) by later accesses.
                state.Layout = op.NewLayout;
                state.WriteStages = stage;
                state.WriteAccess = access & WriteAccessMask;
                state.ReadStages = write ? PipelineStage::None : stage;
                state.VisibleStages = write ? PipelineStage::None : stage;
                state.VisibleAccess = write ? AccessMask::None : access;
                return needed;
            }

            state.ReadStages |= stage;
            if (!HasAny(state.WriteStages))
                return false; // never written, or last synchronized outside the graph

            const bool visible = !HasAny(stage & ~state.VisibleStages) && !HasAny(access & ~state.VisibleAccess);
            if (visible)
                return false;

            op.SrcStage = state.WriteStages;
            op.SrcAccess = state.WriteAccess;
            op.DstStage = state.VisibleStages | stage;
            op.DstAccess = state.VisibleAccess | access;
            op.OldLayout = state.Layout;
            op.NewLayout = state.Layout;
            state.VisibleStages = op.DstStage;
            state.VisibleAccess = op.DstAccess;
            return true;
        }

        // "VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR" -> "RAY_TRACING_SHADER"
        std::string Shorten(std::string text, std::string_view prefix)
        {
            for (std::string_view token : { prefix, std::string_view("_BIT"), std::string_view("_KHR") })
            {
                for (size_t pos = text.find(token); pos != std::string::npos; pos = text.find(token))
                    text.erase(pos, token.size());
            }
            return text;
        }

        std::string StageString(PipelineStage stage)
        {
            return HasAny(stage) ? Shorten(string_VkPipelineStageFlags2(toVk(stage)), "VK_PIPELINE_STAGE_2_") : "NONE";
        }

        std::string AccessString(AccessMask access)
        {
            return HasAny(access) ? Shorten(string_VkAccessFlags2(toVk(access)), "VK_ACCESS_2_") : "NONE";
        }

        std::string LayoutString(ImageLayout layout)
        {
            return Shorten(string_VkImageLayout(toVk(layout)), "VK_IMAGE_LAYOUT_");
        }

        const char* PassTypeName(PassType type)
        {
            switch (type)
            {
            case PassType::Graphics:   return "Graphics";
            case PassType::Compute:    return "Compute";
            case PassType::RayTracing: return "RayTracing";
            case PassType::Transfer:   return "Transfer";
            }
            return "?";
        }

        std::string DescribeBarrier(const std::string& name, const ImageBarrier& barrier)
        {
            std::string layouts = (barrier.OldLayout == barrier.NewLayout)
                ? LayoutString(barrier.NewLayout)
                : LayoutString(barrier.OldLayout) + " -> " + LayoutString(barrier.NewLayout);
            return std::format("        {}: {} | {} / {} -> {} / {}\n", name, layouts,
                StageString(barrier.SrcStage), AccessString(barrier.SrcAccess), StageString(barrier.DstStage), AccessString(barrier.DstAccess));
        }

        std::string DescribeBarrier(const std::string& name, const BufferBarrier& barrier)
        {
            return std::format("        {}: {} / {} -> {} / {}\n", name,
                StageString(barrier.SrcStage), AccessString(barrier.SrcAccess), StageString(barrier.DstStage), AccessString(barrier.DstAccess));
        }
    }

    // PASS BUILDER

    PipelineStage PassBuilder::ShaderStagesFor(std::optional<ShaderStage> stages) const
    {
        if (stages)
            return ToPipelineStages(*stages);

        switch (m_Graph.m_Passes[m_PassIndex].Type)
        {
        case PassType::Graphics:   return PipelineStage::VertexShader | PipelineStage::FragmentShader;
        case PassType::Compute:    return PipelineStage::ComputeShader;
        case PassType::RayTracing: return PipelineStage::RayTracingShader;
        case PassType::Transfer:   return PipelineStage::None;
        }
        return PipelineStage::None;
    }

    void PassBuilder::UseImage(RGImage image, RGImageUsage usage) { UseImageAs(image, usage, std::nullopt); }
    void PassBuilder::UseImage(RGImage image, RGImageUsage usage, ShaderStage stages) { UseImageAs(image, usage, stages); }
    void PassBuilder::UseBuffer(RGBuffer buffer, RGBufferUsage usage) { UseBufferAs(buffer, usage, std::nullopt); }
    void PassBuilder::UseBuffer(RGBuffer buffer, RGBufferUsage usage, ShaderStage stages) { UseBufferAs(buffer, usage, stages); }

    void PassBuilder::UseImageAs(RGImage image, RGImageUsage usage, std::optional<ShaderStage> stages)
    {
        RenderGraph::Pass& pass = m_Graph.m_Passes[m_PassIndex];

        if (usage == RGImageUsage::TransferSrc || usage == RGImageUsage::TransferDst)
        {
            const bool source = usage == RGImageUsage::TransferSrc;
            m_Graph.AddImageAccess(pass, image,
                source ? ImageLayout::TransferSrcOptimal : ImageLayout::TransferDstOptimal,
                PipelineStage::AllTransfer,
                source ? AccessMask::TransferRead : AccessMask::TransferWrite);
            return;
        }

        const PipelineStage shaderStages = ShaderStagesFor(stages);
        if (!HasAny(shaderStages))
        {
            RDN_LOG_ERROR("RenderGraph: pass '{}' declares a shader access to an image, but runs no shaders", pass.Name);
            return;
        }

        ImageLayout layout = ImageLayout::General;
        AccessMask access = AccessMask::None;
        switch (usage)
        {
        case RGImageUsage::Sampled:
            layout = ImageLayout::ShaderReadOnlyOptimal;
            access = AccessMask::ShaderSampledRead;
            break;
        case RGImageUsage::StorageRead:      access = AccessMask::ShaderStorageRead; break;
        case RGImageUsage::StorageWrite:     access = AccessMask::ShaderStorageWrite; break;
        case RGImageUsage::StorageReadWrite: access = AccessMask::ShaderStorageRead | AccessMask::ShaderStorageWrite; break;
        default:                             return;
        }
        m_Graph.AddImageAccess(pass, image, layout, shaderStages, access);
    }

    void PassBuilder::UseBufferAs(RGBuffer buffer, RGBufferUsage usage, std::optional<ShaderStage> stages)
    {
        RenderGraph::Pass& pass = m_Graph.m_Passes[m_PassIndex];

        switch (usage)
        {
        case RGBufferUsage::Vertex:
        case RGBufferUsage::Index:
            if (pass.Type != PassType::Graphics)
            {
                RDN_LOG_ERROR("RenderGraph: pass '{}' binds a vertex/index buffer but is not a graphics pass", pass.Name);
                return;
            }
            if (usage == RGBufferUsage::Vertex)
                return m_Graph.AddBufferAccess(pass, buffer, PipelineStage::VertexAttributeInput, AccessMask::VertexAttributeRead);
            return m_Graph.AddBufferAccess(pass, buffer, PipelineStage::IndexInput, AccessMask::IndexRead);
        case RGBufferUsage::Indirect:
            return m_Graph.AddBufferAccess(pass, buffer, PipelineStage::DrawIndirect, AccessMask::IndirectCommandRead);
        case RGBufferUsage::TransferSrc:
            return m_Graph.AddBufferAccess(pass, buffer, PipelineStage::AllTransfer, AccessMask::TransferRead);
        case RGBufferUsage::TransferDst:
            return m_Graph.AddBufferAccess(pass, buffer, PipelineStage::AllTransfer, AccessMask::TransferWrite);
        default:
            break;
        }

        const PipelineStage shaderStages = ShaderStagesFor(stages);
        if (!HasAny(shaderStages))
        {
            RDN_LOG_ERROR("RenderGraph: pass '{}' declares a shader access to a buffer, but runs no shaders", pass.Name);
            return;
        }

        switch (usage)
        {
        case RGBufferUsage::Uniform:
            return m_Graph.AddBufferAccess(pass, buffer, shaderStages, AccessMask::UniformRead);
        case RGBufferUsage::StorageRead:
            return m_Graph.AddBufferAccess(pass, buffer, shaderStages, AccessMask::ShaderStorageRead);
        case RGBufferUsage::StorageWrite:
            return m_Graph.AddBufferAccess(pass, buffer, shaderStages, AccessMask::ShaderStorageWrite);
        case RGBufferUsage::StorageReadWrite:
            return m_Graph.AddBufferAccess(pass, buffer, shaderStages, AccessMask::ShaderStorageRead | AccessMask::ShaderStorageWrite);
        default:
            return;
        }
    }

    void PassBuilder::UseImage(RGImage image, ImageLayout layout, PipelineStage stage, AccessMask access)
    {
        m_Graph.AddImageAccess(m_Graph.m_Passes[m_PassIndex], image, layout, stage, access);
    }

    void PassBuilder::UseBuffer(RGBuffer buffer, PipelineStage stage, AccessMask access)
    {
        m_Graph.AddBufferAccess(m_Graph.m_Passes[m_PassIndex], buffer, stage, access);
    }

    void PassBuilder::AddColorAttachment(RGImage image, AttachmentLoadOp loadOp, AttachmentStoreOp storeOp, std::array<float, 4> clearColor)
    {
        RenderGraph::Pass& pass = m_Graph.m_Passes[m_PassIndex];
        if (!m_Graph.AddAttachmentExtent(pass, image))
            return;

        // Load reads the previous contents; Clear/DontCare only write
        const bool added = m_Graph.AddImageAccess(pass, image, ImageLayout::ColorAttachmentOptimal, PipelineStage::ColorAttachmentOutput,
            (loadOp == AttachmentLoadOp::Load)
            ? (AccessMask::ColorAttachmentRead | AccessMask::ColorAttachmentWrite)
            : AccessMask::ColorAttachmentWrite);
        if (added)
            pass.ColorAttachments.push_back({ .Image = image, .LoadOp = loadOp, .StoreOp = storeOp, .ClearColor = clearColor });
    }

    void PassBuilder::SetDepthAttachment(RGImage image, AttachmentLoadOp loadOp, AttachmentStoreOp storeOp, float clearDepth)
    {
        RenderGraph::Pass& pass = m_Graph.m_Passes[m_PassIndex];
        if (pass.DepthAttachment)
        {
            RDN_LOG_ERROR("RenderGraph: pass '{}' sets more than one depth attachment", pass.Name);
            return;
        }
        if (image.Index < m_Graph.m_Images.size() && !HasAny(AspectOf(m_Graph.m_Images[image.Index].ImageFormat) & ImageAspect::Depth))
        {
            RDN_LOG_ERROR("RenderGraph: pass '{}' uses '{}' as a depth attachment, but it has no depth aspect", pass.Name, m_Graph.m_Images[image.Index].Name);
            return;
        }
        if (!m_Graph.AddAttachmentExtent(pass, image))
            return;

        // DepthStencilAttachmentOptimal is valid for depth-only formats as well and, unlike
        // DepthAttachmentOptimal, doesn't need the separateDepthStencilLayouts feature.
        const bool added = m_Graph.AddImageAccess(pass, image, ImageLayout::DepthStencilAttachmentOptimal,
            PipelineStage::EarlyFragmentTests | PipelineStage::LateFragmentTests,
            (loadOp == AttachmentLoadOp::Load)
            ? (AccessMask::DepthStencilAttachmentRead | AccessMask::DepthStencilAttachmentWrite)
            : AccessMask::DepthStencilAttachmentWrite);
        if (added)
            pass.DepthAttachment = RenderGraph::AttachmentDecl{ .Image = image, .LoadOp = loadOp, .StoreOp = storeOp, .ClearDepth = clearDepth };
    }

    void PassBuilder::NeverCull()
    {
        m_Graph.m_Passes[m_PassIndex].NeverCull = true;
    }

    // PASS CONTEXT

    ImageHandle PassContext::GetImage(RGImage image) const
    {
        return image.Index < m_Graph->m_Images.size() ? m_Graph->m_Images[image.Index].Image : ImageHandle{};
    }

    ImageViewHandle PassContext::GetImageView(RGImage image) const
    {
        return image.Index < m_Graph->m_Images.size() ? m_Graph->m_Images[image.Index].View : ImageViewHandle{};
    }

    Extent3D PassContext::GetImageExtent(RGImage image) const
    {
        return image.Index < m_Graph->m_Images.size() ? m_Graph->m_Images[image.Index].Extent : Extent3D{};
    }

    BufferHandle PassContext::GetBuffer(RGBuffer buffer) const
    {
        return buffer.Index < m_Graph->m_Buffers.size() ? m_Graph->m_Buffers[buffer.Index].Buffer : BufferHandle{};
    }

    // RENDER GRAPH

    RenderGraph::RenderGraph(Device* device, ResourceAllocator* allocator)
        : m_Device(device)
        , m_Allocator(allocator)
    {
    }

    RenderGraph::~RenderGraph()
    {
        if (!m_TransientPool.empty())
        {
            m_Device->WaitIdle();
            ReleaseTransientResources();
        }
    }

    void RenderGraph::Reset()
    {
        m_Passes.clear();
        m_Images.clear();
        m_Buffers.clear();
        m_ImportedImages.clear();
        m_ImportedBuffers.clear();
        m_FinalBarriers.clear();
        m_FinalBarrierResources.clear();
        m_Executed = false;
    }

    RGImage RenderGraph::ImportImage(std::string name, GpuImage* image)
    {
        if (!image || !image->GetHandle())
        {
            RDN_LOG_ERROR("RenderGraph: can't import image '{}', it hasn't been created", name);
            return {};
        }
        // One logical resource per physical image, or two sync states would diverge
        if (auto it = m_ImportedImages.find(image); it != m_ImportedImages.end())
            return RGImage{ it->second };

        ImageResource& resource = m_Images.emplace_back();
        resource.Name = std::move(name);
        resource.Kind = ResourceKind::Imported;
        resource.External = image;
        resource.Image = image->GetHandle();
        resource.View = image->GetImageView();
        resource.Extent = image->GetImageSize();
        resource.ImageFormat = image->GetFormat();

        const uint32_t index = uint32_t(m_Images.size() - 1);
        m_ImportedImages.emplace(image, index);
        return RGImage{ index };
    }

    RGBuffer RenderGraph::ImportBuffer(std::string name, GpuBuffer* buffer)
    {
        if (!buffer || !buffer->GetHandle())
        {
            RDN_LOG_ERROR("RenderGraph: can't import buffer '{}', it hasn't been created", name);
            return {};
        }
        if (auto it = m_ImportedBuffers.find(buffer); it != m_ImportedBuffers.end())
            return RGBuffer{ it->second };

        BufferResource& resource = m_Buffers.emplace_back();
        resource.Name = std::move(name);
        resource.External = buffer;
        resource.Buffer = buffer->GetHandle();

        const uint32_t index = uint32_t(m_Buffers.size() - 1);
        m_ImportedBuffers.emplace(buffer, index);
        return RGBuffer{ index };
    }

    RGImage RenderGraph::ImportBackbuffer(const FrameContext& frame)
    {
        for (uint32_t i = 0; i < m_Images.size(); i++)
        {
            if (m_Images[i].Kind == ResourceKind::Backbuffer)
                return RGImage{ i };
        }

        ImageResource& resource = m_Images.emplace_back();
        resource.Name = "Backbuffer";
        resource.Kind = ResourceKind::Backbuffer;
        resource.Image = frame.PresentImage;
        resource.View = frame.PresentImageView;
        resource.Extent = Extent3D(frame.PresentExtent);
        resource.ImageFormat = frame.PresentFormat;
        // The presentation engine's last read of this image is ordered before the acquire semaphore
        // wait. Treating that wait's stage as the previous "writer" makes the first barrier's source
        // scope chain with it, so the initial layout transition can't start before the acquire.
        resource.BackbufferState.WriteStages = PresentQueue::AcquireWaitStage;
        return RGImage{ uint32_t(m_Images.size() - 1) };
    }

    RGImage RenderGraph::CreateImage(std::string name, const RGImageDesc& desc)
    {
        if (desc.Size.Width == 0 || desc.Size.Height == 0 || desc.Size.Depth == 0 || desc.Format == Format::Undefined || desc.MipLevels == 0)
        {
            RDN_LOG_ERROR("RenderGraph: transient image '{}' needs a non-zero size, mip count and a format", name);
            return {};
        }

        ImageResource& resource = m_Images.emplace_back();
        resource.Name = std::move(name);
        resource.Kind = ResourceKind::Transient;
        resource.Desc = desc;
        resource.Extent = desc.Size;
        resource.ImageFormat = desc.Format;
        return RGImage{ uint32_t(m_Images.size() - 1) };
    }

    void RenderGraph::AddPass(std::string name, PassType type, PassSetupFn setup, PassExecuteFn execute)
    {
        if (m_Executed)
        {
            RDN_LOG_ERROR("RenderGraph: pass '{}' added after Execute(), call Reset() first", name);
            return;
        }

        Pass& pass = m_Passes.emplace_back();
        pass.Name = std::move(name);
        pass.Type = type;
        pass.Execute = std::move(execute);

        // The builder refers to the pass by index: setup may register more resources (or passes)
        if (setup)
        {
            PassBuilder builder(*this, uint32_t(m_Passes.size() - 1));
            setup(builder);
        }
    }

    bool RenderGraph::AddImageAccess(Pass& pass, RGImage image, ImageLayout layout, PipelineStage stage, AccessMask access)
    {
        if (image.Index >= m_Images.size())
        {
            RDN_LOG_ERROR("RenderGraph: pass '{}' uses an invalid image handle", pass.Name);
            return false;
        }

        // Several uses of one image in a pass become one access, so they get one barrier
        for (ResourceAccess& existing : pass.ImageAccesses)
        {
            if (existing.Resource != image.Index)
                continue;

            if (existing.Layout != layout)
            {
                if (IsAttachmentLayout(existing.Layout) || IsAttachmentLayout(layout))
                {
                    RDN_LOG_ERROR("RenderGraph: pass '{}' uses '{}' both as an attachment and in another way", pass.Name, m_Images[image.Index].Name);
                    return false;
                }
                existing.Layout = ImageLayout::General; // e.g. sampled + storage: General supports both
            }
            existing.Stage |= stage;
            existing.Access |= access;
            return true;
        }

        pass.ImageAccesses.push_back({ .Resource = image.Index, .Layout = layout, .Stage = stage, .Access = access });
        return true;
    }

    void RenderGraph::AddBufferAccess(Pass& pass, RGBuffer buffer, PipelineStage stage, AccessMask access)
    {
        if (buffer.Index >= m_Buffers.size())
        {
            RDN_LOG_ERROR("RenderGraph: pass '{}' uses an invalid buffer handle", pass.Name);
            return;
        }

        for (ResourceAccess& existing : pass.BufferAccesses)
        {
            if (existing.Resource == buffer.Index)
            {
                existing.Stage |= stage;
                existing.Access |= access;
                return;
            }
        }

        pass.BufferAccesses.push_back({ .Resource = buffer.Index, .Stage = stage, .Access = access });
    }

    bool RenderGraph::AddAttachmentExtent(Pass& pass, RGImage image)
    {
        if (pass.Type != PassType::Graphics)
        {
            RDN_LOG_ERROR("RenderGraph: pass '{}' declares an attachment, but only graphics passes can render", pass.Name);
            return false;
        }
        if (image.Index >= m_Images.size())
        {
            RDN_LOG_ERROR("RenderGraph: pass '{}' uses an invalid image handle as an attachment", pass.Name);
            return false;
        }

        const Extent2D extent = m_Images[image.Index].Extent.To2D();
        if (pass.RenderArea.Width == 0 && pass.RenderArea.Height == 0)
        {
            pass.RenderArea = extent;
            return true;
        }
        if (pass.RenderArea != extent)
        {
            RDN_LOG_ERROR("RenderGraph: pass '{}' has attachments of different sizes ({}x{} vs {}x{})", pass.Name,
                pass.RenderArea.Width, pass.RenderArea.Height, extent.Width, extent.Height);
            return false;
        }
        return true;
    }

    ResourceSyncState& RenderGraph::SyncStateOf(ImageResource& image)
    {
        switch (image.Kind)
        {
        case ResourceKind::Imported:  return image.External->m_SyncState;
        case ResourceKind::Transient: return m_TransientPool[image.Physical].State;
        default:                      return image.BackbufferState;
        }
    }

    void RenderGraph::WarnOnce(const std::string& message)
    {
        if (m_Warnings.insert(message).second)
        {
            RDN_LOG_WARNING("{}", message);
        }
    }

    void RenderGraph::Execute(CommandBuffer& cmd, uint32_t frameIndex)
    {
        if (m_Executed)
        {
            RDN_LOG_ERROR("RenderGraph: Execute() called twice for one frame, call Reset() in between");
            return;
        }
        m_Executed = true;
        m_FrameNumber++;

        CullPasses();
        AllocateTransients();
        PlanBarriers();
        RecordPasses(cmd, frameIndex);
    }

    void RenderGraph::CullPasses()
    {
        for (Pass& pass : m_Passes)
            pass.Culled = false;
        if (!m_CullingEnabled)
            return;

        // Walk backwards: a pass survives if it writes something that outlives the frame, or
        // something a surviving later pass reads. Only transients can end up unread.
        std::vector<bool> imageRead(m_Images.size(), false);
        for (int32_t p = int32_t(m_Passes.size()) - 1; p >= 0; p--)
        {
            Pass& pass = m_Passes[p];

            bool writesAnything = std::ranges::any_of(pass.BufferAccesses, [](const ResourceAccess& a) { return IsWrite(a.Access); });
            bool needed = pass.NeverCull || writesAnything; // buffers are always imported
            for (const ResourceAccess& access : pass.ImageAccesses)
            {
                if (!IsWrite(access.Access))
                    continue;
                writesAnything = true;
                needed = needed || m_Images[access.Resource].Kind != ResourceKind::Transient || imageRead[access.Resource];
            }
            if (!writesAnything)
                needed = true; // no declared outputs: its effects are invisible to the graph

            pass.Culled = !needed;
            if (pass.Culled)
                continue;

            // Reads, including the read half of a read-modify-write (e.g. an attachment with Load)
            for (const ResourceAccess& access : pass.ImageAccesses)
            {
                if (IsRead(access.Access) || !IsWrite(access.Access))
                    imageRead[access.Resource] = true;
            }
        }
    }

    void RenderGraph::AllocateTransients()
    {
        // Retire pooled images unused for longer than any frame in flight could still reference them
        const uint64_t retireAfter = uint64_t(m_Device->GetFramesInFlightCount()) + 1;
        for (size_t i = 0; i < m_TransientPool.size();)
        {
            if (m_FrameNumber - m_TransientPool[i].LastUsedFrame > retireAfter)
            {
                m_Allocator->ReleaseResource(m_TransientPool[i].Image.get());
                m_TransientPool.erase(m_TransientPool.begin() + i);
            }
            else
            {
                i++;
            }
        }

        // Lifetimes and usage flags over the passes that survived culling
        for (ImageResource& image : m_Images)
        {
            image.Usage = {};
            image.FirstPass = -1;
            image.LastPass = -1;
        }
        for (int32_t p = 0; p < int32_t(m_Passes.size()); p++)
        {
            if (m_Passes[p].Culled)
                continue;
            for (const ResourceAccess& access : m_Passes[p].ImageAccesses)
            {
                ImageResource& image = m_Images[access.Resource];
                if (image.FirstPass < 0)
                    image.FirstPass = p;
                image.LastPass = p;
                image.Usage |= UsageFor(access.Layout, access.Access);
            }
        }

        // Bind transients to pooled images in order of first use. A pooled image hosts several
        // transients per frame as long as their pass ranges don't overlap.
        std::vector<uint32_t> order;
        for (uint32_t i = 0; i < m_Images.size(); i++)
        {
            if (m_Images[i].Kind == ResourceKind::Transient && m_Images[i].FirstPass >= 0)
                order.push_back(i);
        }
        std::ranges::sort(order, {}, [this](uint32_t i) { return m_Images[i].FirstPass; });

        for (uint32_t index : order)
        {
            ImageResource& image = m_Images[index];
            if (!HasAny(image.Usage))
            {
                WarnOnce(std::format("RenderGraph: no usage flags could be inferred for transient image '{}'", image.Name));
                image.Usage = ImageUsage::TransferDst;
            }

            int32_t chosen = -1;
            for (size_t i = 0; i < m_TransientPool.size(); i++)
            {
                const PooledImage& entry = m_TransientPool[i];
                const bool free = entry.LastUsedFrame != m_FrameNumber || entry.BusyUntilPass < image.FirstPass;
                const bool compatible = entry.Desc.Size == image.Desc.Size
                    && entry.Desc.Format == image.Desc.Format
                    && entry.Desc.MipLevels == image.Desc.MipLevels
                    && (entry.Usage & image.Usage) == image.Usage;
                if (free && compatible)
                {
                    chosen = int32_t(i);
                    break;
                }
            }

            if (chosen < 0)
            {
                PooledImage& entry = m_TransientPool.emplace_back();
                entry.Image = std::make_unique<GpuImage>();
                entry.Desc = image.Desc;
                entry.Usage = image.Usage;
                m_Allocator->CreateGpuImage(entry.Image.get(), GpuImageDesc{
                    .ImageSize = image.Desc.Size,
                    .Format = image.Desc.Format,
                    .UsageFlags = image.Usage,
                    .AspectFlags = AspectOf(image.Desc.Format),
                    .MipLevels = image.Desc.MipLevels,
                });
                chosen = int32_t(m_TransientPool.size() - 1);
            }

            PooledImage& entry = m_TransientPool[chosen];
            entry.LastUsedFrame = m_FrameNumber;
            entry.BusyUntilPass = image.LastPass;
            image.Physical = chosen;
            image.Image = entry.Image->GetHandle();
            image.View = entry.Image->GetImageView();
        }
    }

    void RenderGraph::PlanBarriers()
    {
        for (int32_t p = 0; p < int32_t(m_Passes.size()); p++)
        {
            Pass& pass = m_Passes[p];
            pass.ImageBarriers.clear();
            pass.BufferBarriers.clear();
            pass.ImageBarrierResources.clear();
            pass.BufferBarrierResources.clear();
            if (pass.Culled)
                continue;

            for (const ResourceAccess& access : pass.ImageAccesses)
            {
                ImageResource& image = m_Images[access.Resource];
                ResourceSyncState& state = SyncStateOf(image);

                if (image.Kind == ResourceKind::Transient && p == image.FirstPass)
                {
                    if (!IsWrite(access.Access))
                        WarnOnce(std::format("RenderGraph: pass '{}' reads transient image '{}' before anything writes it", pass.Name, image.Name));

                    // A transient starts out undefined. Keep the previous user's stages so the first
                    // transition still waits for it (WAR/WAW on the shared pooled image).
                    state.Layout = ImageLayout::Undefined;
                    state.VisibleStages = PipelineStage::None;
                    state.VisibleAccess = AccessMask::None;
                }

                SyncOp op;
                if (AdvanceSyncState(state, true, access.Layout, access.Stage, access.Access, op))
                {
                    pass.ImageBarriers.push_back(ImageBarrier{
                        .Handle = image.Image,
                        .OldLayout = op.OldLayout,
                        .NewLayout = op.NewLayout,
                        .SrcStage = op.SrcStage,
                        .DstStage = op.DstStage,
                        .SrcAccess = op.SrcAccess,
                        .DstAccess = op.DstAccess,
                        .Aspect = AspectOf(image.ImageFormat),
                    });
                    pass.ImageBarrierResources.push_back(access.Resource);
                }
            }

            for (const ResourceAccess& access : pass.BufferAccesses)
            {
                BufferResource& buffer = m_Buffers[access.Resource];

                SyncOp op;
                if (AdvanceSyncState(buffer.External->m_SyncState, false, ImageLayout::Undefined, access.Stage, access.Access, op))
                {
                    pass.BufferBarriers.push_back(BufferBarrier{
                        .Handle = buffer.Buffer,
                        .SrcStage = op.SrcStage,
                        .DstStage = op.DstStage,
                        .SrcAccess = op.SrcAccess,
                        .DstAccess = op.DstAccess,
                    });
                    pass.BufferBarrierResources.push_back(access.Resource);
                }
            }
        }

        // Hand the backbuffer to the presentation engine after its last use. The present waits on a
        // semaphore signaled with AllCommands, so there's nothing to put in the destination scope.
        for (uint32_t i = 0; i < m_Images.size(); i++)
        {
            ImageResource& image = m_Images[i];
            if (image.Kind != ResourceKind::Backbuffer)
                continue;

            const ResourceSyncState& state = image.BackbufferState;
            m_FinalBarriers.push_back(ImageBarrier{
                .Handle = image.Image,
                .OldLayout = state.Layout,
                .NewLayout = ImageLayout::PresentSrc,
                .SrcStage = state.WriteStages | state.ReadStages,
                .DstStage = PipelineStage::None,
                .SrcAccess = state.WriteAccess,
                .DstAccess = AccessMask::None,
                .Aspect = ImageAspect::Color,
            });
            m_FinalBarrierResources.push_back(i);
        }
    }

    void RenderGraph::RecordPasses(CommandBuffer& cmd, uint32_t frameIndex)
    {
        const bool debugLabels = m_Device->IsDebugUtilsEnabled();

        PassContext context;
        context.m_Graph = this;
        context.FrameIndex = frameIndex;

        std::vector<ColorAttachment> colorAttachments;
        for (Pass& pass : m_Passes)
        {
            if (pass.Culled)
                continue;

            if (debugLabels)
                cmd.BeginDebugLabel(pass.Name.c_str());

            if (!pass.ImageBarriers.empty() || !pass.BufferBarriers.empty())
                cmd.Barrier(pass.ImageBarriers, pass.BufferBarriers);

            const bool rendering = !pass.ColorAttachments.empty() || pass.DepthAttachment.has_value();
            if (rendering)
            {
                colorAttachments.clear();
                for (const AttachmentDecl& attachment : pass.ColorAttachments)
                {
                    colorAttachments.push_back(ColorAttachment{
                        .ImageView = m_Images[attachment.Image.Index].View,
                        .ImageLayout = ImageLayout::ColorAttachmentOptimal,
                        .LoadOp = attachment.LoadOp,
                        .StoreOp = attachment.StoreOp,
                        .ClearColor = attachment.ClearColor,
                    });
                }

                std::optional<DepthAttachment> depthAttachment;
                if (pass.DepthAttachment)
                {
                    depthAttachment = DepthAttachment{
                        .ImageView = m_Images[pass.DepthAttachment->Image.Index].View,
                        .ImageLayout = ImageLayout::DepthStencilAttachmentOptimal,
                        .LoadOp = pass.DepthAttachment->LoadOp,
                        .StoreOp = pass.DepthAttachment->StoreOp,
                        .ClearDepth = pass.DepthAttachment->ClearDepth,
                    };
                }

                cmd.BeginRendering(colorAttachments, depthAttachment, pass.RenderArea);
                cmd.SetViewport(pass.RenderArea);
                cmd.SetScissors(pass.RenderArea);
            }

            context.RenderArea = pass.RenderArea;
            if (pass.Execute)
                pass.Execute(cmd, context);

            if (rendering)
                cmd.EndRendering();

            if (debugLabels)
                cmd.EndDebugLabel();
        }

        if (!m_FinalBarriers.empty())
            cmd.Barrier(m_FinalBarriers, {});
    }

    void RenderGraph::ReleaseTransientResources()
    {
        for (PooledImage& entry : m_TransientPool)
            m_Allocator->ReleaseResource(entry.Image.get());
        m_TransientPool.clear();

        for (ImageResource& image : m_Images)
        {
            if (image.Kind == ResourceKind::Transient)
            {
                image.Image = {};
                image.View = {};
                image.Physical = -1;
            }
        }
    }

    std::string RenderGraph::DescribeCompiledFrame() const
    {
        if (!m_Executed)
            return "RenderGraph: nothing has been executed since the last Reset()";

        const auto culledCount = std::ranges::count_if(m_Passes, [](const Pass& pass) { return pass.Culled; });
        std::string out = std::format("RenderGraph frame {}: {} passes ({} culled), {} images, {} buffers, {} pooled transient images\n",
            m_FrameNumber, m_Passes.size(), culledCount, m_Images.size(), m_Buffers.size(), m_TransientPool.size());

        for (size_t p = 0; p < m_Passes.size(); p++)
        {
            const Pass& pass = m_Passes[p];
            out += std::format("    [{}] {} ({}", p, pass.Name, PassTypeName(pass.Type));
            if (pass.RenderArea.Width != 0)
                out += std::format(", {}x{}", pass.RenderArea.Width, pass.RenderArea.Height);
            out += pass.Culled ? ") culled: nothing reads what it writes\n" : ")\n";

            for (size_t b = 0; b < pass.ImageBarriers.size(); b++)
                out += DescribeBarrier(m_Images[pass.ImageBarrierResources[b]].Name, pass.ImageBarriers[b]);
            for (size_t b = 0; b < pass.BufferBarriers.size(); b++)
                out += DescribeBarrier(m_Buffers[pass.BufferBarrierResources[b]].Name, pass.BufferBarriers[b]);
        }

        if (!m_FinalBarriers.empty())
        {
            out += "    [end of frame]\n";
            for (size_t b = 0; b < m_FinalBarriers.size(); b++)
                out += DescribeBarrier(m_Images[m_FinalBarrierResources[b]].Name, m_FinalBarriers[b]);
        }

        for (const ImageResource& image : m_Images)
        {
            if (image.Kind != ResourceKind::Transient)
                continue;
            if (image.Physical < 0)
            {
                out += std::format("    transient '{}': not used by any surviving pass\n", image.Name);
                continue;
            }
            out += std::format("    transient '{}' {}x{}x{} {}: pooled image #{}, passes {}..{}\n", image.Name,
                image.Desc.Size.Width, image.Desc.Size.Height, image.Desc.Size.Depth,
                Shorten(string_VkFormat(toVk(image.Desc.Format)), "VK_FORMAT_"), image.Physical, image.FirstPass, image.LastPass);
        }
        return out;
    }
}

#pragma once
#include "Handle.h"
#include "Device.h"
#include "Common.h"
#include "CommandBuffer.h"
#include "Attachment.h"
#include "PresentQueue.h"
#include "Resources/GpuImage.h"
#include "Resources/GpuBuffer.h"

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Rdn
{
	class ResourceAllocator;
	class RenderGraph;

	// Frame-local handles to resources registered with a RenderGraph. Valid until the next Reset().
	struct RGImage
	{
		uint32_t Index = UINT32_MAX;
		bool Valid() const { return Index != UINT32_MAX; }
		explicit operator bool() const { return Valid(); }
	};

	struct RGBuffer
	{
		uint32_t Index = UINT32_MAX;
		bool Valid() const { return Index != UINT32_MAX; }
		explicit operator bool() const { return Valid(); }
	};

	enum class PassType
	{
		Graphics,   // may declare attachments; Execute runs inside BeginRendering/EndRendering
		Compute,
		RayTracing,
		Transfer,
	};

	// How a pass uses an image. The graph derives the layout, stages and access masks from it.
	enum class RGImageUsage
	{
		Sampled,          // read through a sampler             -> ShaderReadOnlyOptimal
		StorageRead,      // imageLoad                          -> General
		StorageWrite,     // imageStore                         -> General
		StorageReadWrite, // imageLoad + imageStore             -> General
		TransferSrc,      // copy / blit source                 -> TransferSrcOptimal
		TransferDst,      // copy / blit / clear destination    -> TransferDstOptimal
	};

	// How a pass uses a buffer.
	enum class RGBufferUsage
	{
		Uniform,
		StorageRead,
		StorageWrite,
		StorageReadWrite,
		Vertex,
		Index,
		Indirect,
		TransferSrc,
		TransferDst,
	};

	// A graph-owned image whose contents only live within one frame. Usage flags are inferred
	// from the passes that use it. Backing images are pooled across frames, and transients whose
	// pass ranges don't overlap share one.
	struct RGImageDesc
	{
		Extent3D Size = {};
		Format   Format = Format::Undefined;
		uint32_t MipLevels = 1;
	};

	class PassBuilder
	{
	public:
		// Shader usages apply to the stages implied by the pass type (Graphics: vertex + fragment,
		// Compute: compute, RayTracing: all ray tracing stages) unless `stages` narrows them.
		void UseImage(RGImage image, RGImageUsage usage);
		void UseImage(RGImage image, RGImageUsage usage, ShaderStage stages);
		void UseBuffer(RGBuffer buffer, RGBufferUsage usage);
		void UseBuffer(RGBuffer buffer, RGBufferUsage usage, ShaderStage stages);

		void UseImage(RGImage image, ImageLayout layout, PipelineStage stage, AccessMask access);
		void UseBuffer(RGBuffer buffer, PipelineStage stage, AccessMask access);

		void AddColorAttachment(RGImage image,
			AttachmentLoadOp loadOp = AttachmentLoadOp::Clear,
			AttachmentStoreOp storeOp = AttachmentStoreOp::Store,
			std::array<float, 4> clearColor = { 0.0f, 0.0f, 0.0f, 1.0f });

		void SetDepthAttachment(RGImage image,
			AttachmentLoadOp loadOp = AttachmentLoadOp::Clear,
			AttachmentStoreOp storeOp = AttachmentStoreOp::DontCare,
			float clearDepth = 1.0f);

		// Keeps the pass even if nothing reads what it writes (side effects the graph can't see).
		void NeverCull();

	private:
		friend class RenderGraph;
		PassBuilder(RenderGraph& graph, uint32_t passIndex) : m_Graph(graph), m_PassIndex(passIndex) {}

		void UseImageAs(RGImage image, RGImageUsage usage, std::optional<ShaderStage> stages);
		void UseBufferAs(RGBuffer buffer, RGBufferUsage usage, std::optional<ShaderStage> stages);
		PipelineStage ShaderStagesFor(std::optional<ShaderStage> stages) const;

		RenderGraph& m_Graph;
		uint32_t m_PassIndex;
	};

	class PassContext
	{
	public:
		uint32_t FrameIndex = 0;
		Extent2D RenderArea = {};

		ImageHandle     GetImage(RGImage image) const;
		ImageViewHandle GetImageView(RGImage image) const;
		Extent3D        GetImageExtent(RGImage image) const;
		BufferHandle    GetBuffer(RGBuffer buffer) const;

	private:
		friend class RenderGraph;
		const RenderGraph* m_Graph = nullptr;
	};

	using PassSetupFn = std::function<void(PassBuilder&)>;
	using PassExecuteFn = std::function<void(CommandBuffer&, const PassContext&)>;

	// Per frame: Reset(), register resources, AddPass() in execution order, Execute().
	class RenderGraph
	{
	public:
		RenderGraph(Device* device, ResourceAllocator* allocator);
		~RenderGraph();
		RenderGraph(const RenderGraph&) = delete;
		RenderGraph& operator=(const RenderGraph&) = delete;

		void Reset();

		// Caller-owned resources. Their sync state is stored on the resource itself, so barriers
		// carry across frames and restart from scratch when the resource is recreated.
		RGImage  ImportImage(std::string name, GpuImage* image);
		RGBuffer ImportBuffer(std::string name, GpuBuffer* buffer);

		// This frame's swapchain image. Its first use is ordered after the acquire, and it is
		// transitioned to PresentSrc after its last use.
		RGImage  ImportBackbuffer(const FrameContext& frame);

		// Graph-owned image whose contents only live within this frame.
		RGImage  CreateImage(std::string name, const RGImageDesc& desc);

		void AddPass(std::string name, PassType type, PassSetupFn setup, PassExecuteFn execute);

		void Execute(CommandBuffer& cmd, uint32_t frameIndex);
		void ReleaseTransientResources();

		std::string DescribeCompiledFrame() const;

		void SetCullingEnabled(bool enabled) { m_CullingEnabled = enabled; }

	private:
		friend class PassBuilder;
		friend class PassContext;

		enum class ResourceKind { Imported, Backbuffer, Transient };

		struct ImageResource
		{
			std::string       Name;
			ResourceKind      Kind = ResourceKind::Imported;
			GpuImage*         External = nullptr;
			RGImageDesc       Desc = {};
			ImageHandle       Image = {};
			ImageViewHandle   View = {};
			Extent3D          Extent = {};
			Format            ImageFormat = Format::Undefined;
			ResourceSyncState BackbufferState = {};

			ImageUsage Usage = {};
			int32_t    FirstPass = -1;
			int32_t    LastPass = -1;
			int32_t    Physical = -1;
		};

		struct BufferResource
		{
			std::string  Name;
			GpuBuffer*   External = nullptr;
			BufferHandle Buffer = {};
		};

		struct ResourceAccess
		{
			uint32_t      Resource = 0;
			ImageLayout   Layout = ImageLayout::Undefined;
			PipelineStage Stage = PipelineStage::None;
			AccessMask    Access = AccessMask::None;
		};

		struct AttachmentDecl
		{
			RGImage              Image;
			AttachmentLoadOp     LoadOp = AttachmentLoadOp::Clear;
			AttachmentStoreOp    StoreOp = AttachmentStoreOp::Store;
			std::array<float, 4> ClearColor = {};
			float                ClearDepth = 1.0f;
		};

		struct Pass
		{
			std::string   Name;
			PassType      Type = PassType::Graphics;
			PassExecuteFn Execute;
			std::vector<ResourceAccess>   ImageAccesses;
			std::vector<ResourceAccess>   BufferAccesses;
			std::vector<AttachmentDecl>   ColorAttachments;
			std::optional<AttachmentDecl> DepthAttachment;
			Extent2D      RenderArea = {};
			bool          NeverCull = false;

			bool                       Culled = false;
			std::vector<ImageBarrier>  ImageBarriers;
			std::vector<BufferBarrier> BufferBarriers;
			std::vector<uint32_t>      ImageBarrierResources;
			std::vector<uint32_t>      BufferBarrierResources;
		};

		struct PooledImage
		{
			std::unique_ptr<GpuImage> Image;
			RGImageDesc       Desc = {};
			ImageUsage        Usage = {};
			ResourceSyncState State = {};
			uint64_t          LastUsedFrame = 0;
			int32_t           BusyUntilPass = -1;
		};

		bool AddImageAccess(Pass& pass, RGImage image, ImageLayout layout, PipelineStage stage, AccessMask access);
		void AddBufferAccess(Pass& pass, RGBuffer buffer, PipelineStage stage, AccessMask access);
		bool AddAttachmentExtent(Pass& pass, RGImage image);

		void CullPasses();
		void AllocateTransients();
		void PlanBarriers();
		void RecordPasses(CommandBuffer& cmd, uint32_t frameIndex);

		ResourceSyncState& SyncStateOf(ImageResource& image);
		void WarnOnce(const std::string& message);

		Device* m_Device;
		ResourceAllocator* m_Allocator;

		std::vector<Pass>           m_Passes;
		std::vector<ImageResource>  m_Images;
		std::vector<BufferResource> m_Buffers;
		std::unordered_map<GpuImage*, uint32_t>  m_ImportedImages;
		std::unordered_map<GpuBuffer*, uint32_t> m_ImportedBuffers;
		std::vector<ImageBarrier>   m_FinalBarriers;
		std::vector<uint32_t>       m_FinalBarrierResources;

		std::vector<PooledImage>    m_TransientPool;
		uint64_t m_FrameNumber = 0;
		bool     m_Executed = false;
		bool     m_CullingEnabled = true;
		std::unordered_set<std::string> m_Warnings;
	};
}

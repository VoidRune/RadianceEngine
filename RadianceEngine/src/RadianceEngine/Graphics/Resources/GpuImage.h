#pragma once
#include "RadianceEngine/Graphics/Handle.h"
#include "RadianceEngine/Graphics/Common.h"

namespace Rdn
{
	struct GpuImageDesc
	{
		Extent3D ImageSize = { 0, 0, 1 };
		Format Format = Format::Undefined;
		ImageUsage UsageFlags = {};
		ImageAspect AspectFlags = ImageAspect::Color;
		uint32_t MipLevels = 1;
	};

	class GpuImage : NonCopyable
	{
	public:
		ImageHandle GetHandle() const { return m_Image; }
		ImageViewHandle GetImageView() const { return m_ImageView; }
		Format GetFormat() const { return m_Format; }
		Extent3D GetImageSize() const { return m_ImageSize; }
		uint32_t GetMipLevels() const { return m_MipLevels; }
		ImageLayout GetLayout() const { return m_SyncState.Layout; }

	private:
		ImageHandle m_Image{};
		ImageViewHandle m_ImageView{};
		AllocationHandle m_Allocation{};
		Format m_Format = Format::Undefined;
		Extent3D m_ImageSize{};
		uint32_t m_MipLevels = 1;
		ResourceSyncState m_SyncState{};

		friend class ResourceAllocator;
		friend class RenderGraph;
	};
}

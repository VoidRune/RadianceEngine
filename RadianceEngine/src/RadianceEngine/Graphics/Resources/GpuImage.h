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

	class GpuImage
	{
	public:
		ImageHandle GetHandle() { return m_Image; }
		ImageViewHandle GetImageView() { return m_ImageView; }
		AllocationHandle GetDeviceMemory() { return m_Allocation; }
		Format GetFormat() { return m_Format; }
		Extent3D GetImageSize() { return m_ImageSize; }
		uint32_t GetMipLevels() { return m_MipLevels; }
		ImageLayout GetLayout() const { return m_SyncState.Layout; }

	private:
		ImageHandle m_Image;
		ImageViewHandle m_ImageView;
		AllocationHandle m_Allocation;
		Format m_Format;
		Extent3D m_ImageSize;
		uint32_t m_MipLevels;
		ResourceSyncState m_SyncState;

		friend class ResourceAllocator;
		friend class RenderGraph;
	};
}
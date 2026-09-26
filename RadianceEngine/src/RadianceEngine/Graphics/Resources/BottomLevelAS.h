#pragma once
#include "RadianceEngine/Graphics/Handle.h"
#include "RadianceEngine/Graphics/Common.h"
#include <vector>

namespace Rdn
{
	struct BottomLevelASGeometry
	{
		uint32_t FirstIndex = 0;
		uint32_t TriangleCount = 0;
	};

	struct BottomLevelASDesc
	{
		BufferHandle VertexBuffer = {};
		BufferHandle IndexBuffer = {};
		uint32_t VertexStride = 0;
		uint32_t VertexCount = 0;
		Format VertexFormat = Format::Undefined;
		std::vector<BottomLevelASGeometry> Geometries;
	};

	class BottomLevelAS : NonCopyable
	{
	public:
		AccelerationStructureHandle GetHandle() const { return m_Handle; }
		uint64_t GetDeviceAddress() const { return m_DeviceAddress; }

	private:
		AccelerationStructureHandle m_Handle{};
		uint64_t m_DeviceAddress{};
		BufferHandle m_Buffer{};
		AllocationHandle m_Allocation{};

		friend class ResourceAllocator;
	};
}

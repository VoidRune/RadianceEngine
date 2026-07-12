#pragma once
#include "RadianceEngine/Graphics/Handle.h"
#include "RadianceEngine/Graphics/Common.h"

namespace Rdn
{
	struct BottomLevelASDesc
	{
		BufferHandle VertexBuffer = {};
		BufferHandle IndexBuffer = {};
		uint32_t VertexStride = 0;
		uint32_t VertexCount = 0;
		uint32_t NumTriangles = 0;
		Format VertexFormat = Format::Undefined;
	};

	typedef uint64_t BottomLevelASHandle;
	class BottomLevelAS
	{
	public:
		BottomLevelASHandle GetHandle() { return m_DeviceAddress; }

	private:
		AccelerationStructureHandle m_Handle{};
		uint64_t m_DeviceAddress{};
		BufferHandle m_Buffer{};
		AllocationHandle m_Allocation{};

		friend class ResourceAllocator;
	};
}
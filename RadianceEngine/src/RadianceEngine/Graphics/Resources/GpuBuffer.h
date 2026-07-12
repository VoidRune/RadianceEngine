#pragma once
#include "RadianceEngine/Graphics/Handle.h"
#include "RadianceEngine/Graphics/Common.h"

namespace Rdn
{
	struct GpuBufferDesc
	{
		uint32_t Size;
		BufferUsage UsageFlags;
		MemoryProperty MemoryProperty;
		bool Mapped = false;
	};

	class GpuBuffer
	{
	public:
		BufferHandle GetHandle() { return m_Buffer; }
		uint32_t GetSize() { return m_Size; }
		void* GetMappedPtr() { return m_MappedPtr; }

	private:
		BufferHandle m_Buffer;
		AllocationHandle m_Allocation;
		uint32_t m_Size;
		void* m_MappedPtr;

		friend class ResourceAllocator;
	};
}
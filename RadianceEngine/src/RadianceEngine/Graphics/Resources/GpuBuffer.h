#pragma once
#include "RadianceEngine/Graphics/Handle.h"
#include "RadianceEngine/Graphics/Common.h"
#include "RadianceEngine/Core/Log.h"

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
		void Write(const void* data, uint32_t size)
		{
			if (size > m_Size)
			{
				RDN_LOG_ERROR("Writing to GpuBuffer is invalid, size [{}] is greater than whole size [{}]", size, m_Size);
				return;
			}
			if (m_MappedPtr == nullptr)
			{
				RDN_LOG_ERROR("Writing to GpuBuffer is invalid, mapped pointer is nullptr");
				return;
			}
			memcpy(m_MappedPtr, data, size);
		}
		template<typename T>
		void Write(const T& value)
		{
			Write(&value, sizeof(T));
		}


		BufferHandle GetHandle() { return m_Buffer; }
		uint32_t GetSize() { return m_Size; }
		void* GetMappedPtr() { return m_MappedPtr; }

	private:
		BufferHandle m_Buffer;
		AllocationHandle m_Allocation;
		uint32_t m_Size = 0;
		void* m_MappedPtr = nullptr;
		ResourceSyncState m_SyncState;

		friend class ResourceAllocator;
		friend class RenderGraph;
	};
}
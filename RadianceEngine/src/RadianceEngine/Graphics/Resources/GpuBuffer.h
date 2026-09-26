#pragma once
#include "RadianceEngine/Graphics/Handle.h"
#include "RadianceEngine/Graphics/Common.h"
#include "RadianceEngine/Core/Log.h"
#include <cstring>

namespace Rdn
{
	struct GpuBufferDesc
	{
		uint64_t Size = 0;
		BufferUsage UsageFlags = {};
		MemoryProperty MemoryProperty = {};
		bool Mapped = false;
	};

	class GpuBuffer : NonCopyable
	{
	public:
		void Write(const void* data, uint64_t size, uint64_t offset = 0)
		{
			if (offset > m_Size || size > m_Size - offset)
			{
				RDN_LOG_ERROR("GpuBuffer::Write: {} bytes at offset {} don't fit into {} bytes", size, offset, m_Size);
				return;
			}
			if (m_MappedPtr == nullptr)
			{
				RDN_LOG_ERROR("GpuBuffer::Write: the buffer is not mapped");
				return;
			}
			memcpy(static_cast<uint8_t*>(m_MappedPtr) + offset, data, size);
		}
		template<typename T>
		void Write(const T& value)
		{
			Write(&value, sizeof(T));
		}

		BufferHandle GetHandle() const { return m_Buffer; }
		uint64_t GetSize() const { return m_Size; }
		void* GetMappedPtr() const { return m_MappedPtr; }

	private:
		BufferHandle m_Buffer{};
		AllocationHandle m_Allocation{};
		uint64_t m_Size = 0;
		void* m_MappedPtr = nullptr;
		ResourceSyncState m_SyncState{};

		friend class ResourceAllocator;
		friend class RenderGraph;
	};
}

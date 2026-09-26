#pragma once
#include "RadianceEngine/Graphics/Handle.h"
#include "RadianceEngine/Graphics/Common.h"
#include "RadianceEngine/Core/Log.h"
#include <cstring>

namespace Rdn
{
	struct GpuRingBufferDesc
	{
		uint32_t ElementSize = 0;
		uint32_t ElementCount = 0;
		BufferUsage UsageFlags = {};
		MemoryProperty MemoryProperty = {};
		bool Mapped = false;
	};

	class GpuRingBuffer : NonCopyable
	{
	public:
		void Write(uint32_t elementIndex, const void* data, uint32_t size)
		{
			if (elementIndex >= m_ElementCount)
			{
				RDN_LOG_ERROR("GpuRingBuffer::Write: element {} is out of range for {} elements", elementIndex, m_ElementCount);
				return;
			}
			if (size > m_ElementSize)
			{
				RDN_LOG_ERROR("GpuRingBuffer::Write: {} bytes don't fit into an element of {} bytes", size, m_ElementSize);
				return;
			}
			if (m_MappedPtr == nullptr)
			{
				RDN_LOG_ERROR("GpuRingBuffer::Write: the buffer is not mapped");
				return;
			}
			memcpy(static_cast<uint8_t*>(m_MappedPtr) + GetOffset(elementIndex), data, size);
		}
		template<typename T>
		void Write(uint32_t elementIndex, const T& value)
		{
			Write(elementIndex, &value, sizeof(T));
		}

		BufferHandle GetHandle() const { return m_Buffer; }
		uint32_t GetWholeSize() const { return m_WholeSize; }
		uint32_t GetElementSize() const { return m_ElementSize; }
		uint32_t GetElementCount() const { return m_ElementCount; }
		uint32_t GetOffset(uint32_t elementIndex) const { return elementIndex * m_ElementSize; }
		void* GetMappedPtr() const { return m_MappedPtr; }

	private:
		BufferHandle m_Buffer{};
		AllocationHandle m_Allocation{};
		uint32_t m_WholeSize = 0;
		uint32_t m_ElementSize = 0;
		uint32_t m_ElementCount = 0;
		void* m_MappedPtr = nullptr;

		friend class ResourceAllocator;
	};
}

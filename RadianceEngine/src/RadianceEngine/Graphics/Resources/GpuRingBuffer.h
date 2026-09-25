#pragma once
#include "RadianceEngine/Graphics/Handle.h"
#include "RadianceEngine/Graphics/Common.h"
#include "RadianceEngine/Core/Log.h"

namespace Rdn
{
	struct GpuRingBufferDesc
	{
		uint32_t ElementSize;
		uint32_t ElementCount;
		BufferUsage UsageFlags;
		MemoryProperty MemoryProperty;
		bool Mapped = false;
	};

	class GpuRingBuffer
	{
	public:
		void Write(uint32_t elementIndex, const void* data, uint32_t size)
		{
			if (elementIndex >= m_ElementCount)
			{
				RDN_LOG_ERROR("Writing to GpuRingBuffer is invalid, index [{}] is invalid for [{}] elements", elementIndex, m_ElementCount);
				return;
			}
			if (size > m_ElementSize)
			{
				RDN_LOG_ERROR("Writing to GpuRingBuffer is invalid, size [{}] is greater than element size [{}]", size, m_ElementSize);
				return;
			}
			if (m_MappedPtr == nullptr)
			{
				RDN_LOG_ERROR("Writing to GpuRingBuffer is invalid, mapped pointer is nullptr");
				return;
			}
			uint8_t* dst = static_cast<uint8_t*>(m_MappedPtr) + GetOffset(elementIndex);
			memcpy(dst, data, size);
		}
		template<typename T>
		void Write(uint32_t elementIndex, const T& value)
		{
			Write(elementIndex, &value, sizeof(T));
		}

		BufferHandle GetHandle() { return m_Buffer; }
		uint32_t GetWholeSize() { return m_WholeSize; }
		uint32_t GetElementSize() { return m_ElementSize; }
		uint32_t GetElementCount() { return m_ElementCount; }
		uint32_t GetOffset(uint32_t elementIndex) { return elementIndex * m_ElementSize; }
		void* GetMappedPtr() { return m_MappedPtr; }

	private:
		BufferHandle m_Buffer;
		AllocationHandle m_Allocation;
		uint32_t m_WholeSize = 0;
		uint32_t m_ElementSize = 0;
		uint32_t m_ElementCount = 0;
		void* m_MappedPtr = nullptr;

		friend class ResourceAllocator;
	};
}
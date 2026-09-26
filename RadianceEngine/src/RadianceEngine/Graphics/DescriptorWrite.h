#pragma once
#include "Handle.h"
#include "Common.h"
#include <vector>

namespace Rdn
{
	struct BufferWrite
	{
		uint32_t Binding = 0;
		DescriptorType Type = {};
		BufferHandle Buffer = {};
		uint64_t Offset = 0;
		uint64_t Size = WholeSize;
		uint32_t ArrayElement = 0;
	};

	struct ImageWrite
	{
		uint32_t Binding = 0;
		DescriptorType Type = {};
		ImageViewHandle ImageView = {};
		ImageLayout ImageLayout = ImageLayout::Undefined;
		SamplerHandle Sampler = {};
		uint32_t ArrayElement = 0;
	};

	struct AccelerationStructureWrite
	{
		uint32_t Binding = 0;
		DescriptorType Type = {};
		AccelerationStructureHandle AccelerationStructure = {};
		uint32_t ArrayElement = 0;
	};

	class DescriptorWrite
	{
	public:
		DescriptorWrite& AddWrite(const BufferWrite& write)
		{
			m_BufferWrites.push_back(write);
			return *this;
		}
		DescriptorWrite& AddWrite(const ImageWrite& write)
		{
			m_ImageWrites.push_back(write);
			return *this;
		}
		DescriptorWrite& AddWrite(const AccelerationStructureWrite& write)
		{
			m_AccelerationStructureWrites.push_back(write);
			return *this;
		}

		const std::vector<BufferWrite>& GetBufferWrites() const { return m_BufferWrites; }
		const std::vector<ImageWrite>& GetImageWrites() const { return m_ImageWrites; }
		const std::vector<AccelerationStructureWrite>& GetAccelerationStructureWrites() const { return m_AccelerationStructureWrites; }

	private:
		std::vector<BufferWrite> m_BufferWrites;
		std::vector<ImageWrite> m_ImageWrites;
		std::vector<AccelerationStructureWrite> m_AccelerationStructureWrites;
	};
}

#pragma once
#include "RadianceEngine/Graphics/Handle.h"
#include "RadianceEngine/Graphics/Common.h"
#include "BottomLevelAS.h"
#include <vector>

namespace Rdn
{
	struct TopLevelASInstance
	{
		BottomLevelASHandle BottomLevelASHandle{};
		uint32_t InstanceCustomIndex{};
		float TransformMatrix[3][4];
	};

	class Device;
	class TopLevelAS
	{
	public:
		void AddInstance(const TopLevelASInstance& instance) { m_Instances.push_back(instance); };
		void ClearInstances() { m_Instances.clear(); };

		AccelerationStructureHandle GetHandle() { return m_Handle; }

	private:
		Device* m_Device;
		DeviceHandle m_LogicalDevice;
		AllocatorHandle m_Allocator;

		std::vector<TopLevelASInstance> m_Instances;

		AccelerationStructureHandle m_Handle{};
		BufferHandle m_Buffer{};
		AllocationHandle m_Allocation{};

		friend class ResourceAllocator;
	};

}
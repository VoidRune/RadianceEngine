#pragma once
#include "RadianceEngine/Graphics/Handle.h"
#include "RadianceEngine/Graphics/Common.h"
#include "BottomLevelAS.h"
#include <vector>

namespace Rdn
{
	struct TopLevelASInstance
	{
		uint64_t BottomLevelASAddress = 0;
		uint32_t InstanceCustomIndex = 0;
		float TransformMatrix[3][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } };
	};

	class TopLevelAS : NonCopyable
	{
	public:
		void AddInstance(const TopLevelASInstance& instance) { m_Instances.push_back(instance); }
		void ClearInstances() { m_Instances.clear(); }
		size_t GetInstanceCount() const { return m_Instances.size(); }

		AccelerationStructureHandle GetHandle() const { return m_Handle; }

	private:
		std::vector<TopLevelASInstance> m_Instances;

		AccelerationStructureHandle m_Handle{};
		BufferHandle m_Buffer{};
		AllocationHandle m_Allocation{};

		friend class ResourceAllocator;
	};
}

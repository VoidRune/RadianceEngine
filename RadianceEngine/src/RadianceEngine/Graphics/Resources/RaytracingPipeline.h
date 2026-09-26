#pragma once
#include "RadianceEngine/Graphics/Handle.h"
#include "RadianceEngine/Graphics/Common.h"
#include "RadianceEngine/Graphics/Resources/Shader.h"

namespace Rdn
{
	struct RayTracingPipelineDesc
	{
		std::vector<Shader*> ShaderStages = {};
		std::vector<uint32_t> PushDescriptorSets = {};
	};

	class RayTracingPipeline : NonCopyable
	{
	public:
		PipelineHandle GetHandle() const { return m_Pipeline; }
		PipelineLayoutHandle GetLayout() const { return m_PipelineLayout; }
		DescriptorSetLayoutHandle GetSetLayout(uint32_t setIndex) const { return setIndex < m_SetLayouts.size() ? m_SetLayouts[setIndex] : DescriptorSetLayoutHandle{}; }

		struct StridedDeviceAddressRegion
		{
			uint64_t DeviceAddress = 0;
			uint64_t Stride = 0;
			uint64_t Size = 0;
		};

		StridedDeviceAddressRegion GetRayGenShaderBindingTable() const { return m_RayGenShaderBindingTable; }
		StridedDeviceAddressRegion GetRayMissShaderBindingTable() const { return m_RayMissShaderBindingTable; }
		StridedDeviceAddressRegion GetRayClosestHitShaderBindingTable() const { return m_RayClosestHitShaderBindingTable; }
	private:

		PipelineHandle m_Pipeline{};
		PipelineLayoutHandle m_PipelineLayout{};
		std::vector<DescriptorSetLayoutHandle> m_SetLayouts;
		BufferHandle m_ShaderBindingTableBuffer{};
		AllocationHandle m_ShaderBindingTableAllocation{};

		StridedDeviceAddressRegion m_RayGenShaderBindingTable;
		StridedDeviceAddressRegion m_RayMissShaderBindingTable;
		StridedDeviceAddressRegion m_RayClosestHitShaderBindingTable;

		friend class ResourceAllocator;
	};
}
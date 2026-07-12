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

	class RayTracingPipeline
	{
	public:
		PipelineHandle GetHandle() { return m_Pipeline; }
		PipelineLayoutHandle GetLayout() { return m_PipelineLayout; }
		DescriptorSetLayoutHandle GetSetLayout(uint32_t setIndex) { return m_SetLayouts[setIndex]; }

		struct StridedDeviceAddressRegion
		{
			uint64_t DeviceAddress;
			uint64_t Stride;
			uint64_t Size;
		};

		StridedDeviceAddressRegion GetRayGenShaderBindingTable() { return m_RayGenShaderBindingTable; }
		StridedDeviceAddressRegion GetRayMissShaderBindingTable() { return m_RayMissShaderBindingTable; }
		StridedDeviceAddressRegion GetRayClosestHitShaderBindingTable() { return m_RayClosestHitShaderBindingTable; }
	private:

		PipelineHandle m_Pipeline;
		PipelineLayoutHandle m_PipelineLayout;
		std::vector<DescriptorSetLayoutHandle> m_SetLayouts;
		BufferHandle m_ShaderBindingTableBuffer;
		AllocationHandle m_ShaderBindingTableAllocation;

		StridedDeviceAddressRegion m_RayGenShaderBindingTable;
		StridedDeviceAddressRegion m_RayMissShaderBindingTable;
		StridedDeviceAddressRegion m_RayClosestHitShaderBindingTable;

		friend class ResourceAllocator;
	};
}
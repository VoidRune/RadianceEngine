#pragma once
#include "RadianceEngine/Graphics/Handle.h"
#include "RadianceEngine/Graphics/Common.h"
#include "RadianceEngine/Graphics/Resources/Shader.h"
#include <vector>

namespace Rdn
{
	struct ComputePipelineDesc
	{
		Shader* ComputeShader = nullptr;
		std::vector<uint32_t> PushDescriptorSets = {};
	};

	class ComputePipeline : NonCopyable
	{
	public:
		PipelineHandle GetHandle() const { return m_Pipeline; }
		PipelineLayoutHandle GetLayout() const { return m_PipelineLayout; }
		DescriptorSetLayoutHandle GetSetLayout(uint32_t setIndex) const { return setIndex < m_SetLayouts.size() ? m_SetLayouts[setIndex] : DescriptorSetLayoutHandle{}; }
		Extent3D GetWorkgroupSize() const { return m_WorkgroupSize; }

	private:
		PipelineHandle m_Pipeline{};
		PipelineLayoutHandle m_PipelineLayout{};
		std::vector<DescriptorSetLayoutHandle> m_SetLayouts;
		Extent3D m_WorkgroupSize{ 1, 1, 1 };

		friend class ResourceAllocator;
	};
}

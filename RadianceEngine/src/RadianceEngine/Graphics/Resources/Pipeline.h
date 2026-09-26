#pragma once
#include "RadianceEngine/Graphics/Handle.h"
#include "RadianceEngine/Graphics/Common.h"
#include "RadianceEngine/Graphics/Resources/Shader.h"

namespace Rdn
{
	class VertexAttributes
	{
	public:
		struct Attribute
		{
			Attribute(Format format, uint32_t offset) : format(format), offset(offset) {}
			Format format;
			uint32_t offset;
		};

		VertexAttributes() : m_Attributes({ }), m_VertexSize(0) {}
		VertexAttributes(const std::vector<Attribute>& attributes, uint32_t vertexSize) : m_Attributes(attributes), m_VertexSize(vertexSize) {}
	private:
		std::vector<Attribute> m_Attributes;
		uint32_t m_VertexSize;

		friend class ResourceAllocator;
	};

	struct PipelineDesc
	{
		std::vector<Shader*> ShaderStages = {};
		PrimitiveTopology Topology = PrimitiveTopology::TriangleList;
		CullMode CullMode = CullMode::Back;
		bool DepthTest = true;
		bool DepthWrite = true;
		CompareOperation DepthCompareOp = CompareOperation::Less;
		VertexAttributes VertexAttributes = {};
		std::vector<Format> ColorAttachmentFormats = {};
		Format DepthAttachmentFormat = Format::Undefined;
		std::vector<uint32_t> PushDescriptorSets = {};
	};

	class Pipeline : NonCopyable
	{
	public:
		PipelineHandle GetHandle() const { return m_Pipeline; }
		PipelineLayoutHandle GetLayout() const { return m_PipelineLayout; }
		DescriptorSetLayoutHandle GetSetLayout(uint32_t setIndex) const { return setIndex < m_SetLayouts.size() ? m_SetLayouts[setIndex] : DescriptorSetLayoutHandle{}; }

	private:
		PipelineHandle m_Pipeline{};
		PipelineLayoutHandle m_PipelineLayout{};
		std::vector<DescriptorSetLayoutHandle> m_SetLayouts;

		friend class ResourceAllocator;
	};
}
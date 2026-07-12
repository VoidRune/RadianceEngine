#pragma once
#include "RadianceEngine/Graphics/Handle.h"

namespace Rdn
{
	// A persistently-allocated descriptor set (as opposed to a push descriptor set,
	// which is recorded directly into a command buffer and never stored). Used for
	// bindings that must survive across frames and be updated in place -- most
	// notably a bindless resource heap, where VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
	// lets ResourceAllocator::UpdateDescriptorSet write new entries into an already-
	// allocated set without needing per-frame-in-flight copies of it.
	class DescriptorSet
	{
	public:
		DescriptorSetHandle GetHandle() { return m_DescriptorSet; }

	private:
		DescriptorSetHandle m_DescriptorSet;

		friend class ResourceAllocator;
	};
}

#pragma once
#include "RadianceEngine/Graphics/Handle.h"
#include "RadianceEngine/Graphics/Common.h"

namespace Rdn
{
	struct SamplerDesc
	{
		Filter MinFilter = Filter::Linear;
		Filter MagFilter = Filter::Linear;
		SamplerAddressMode AddressMode = SamplerAddressMode::MirroredRepeat;
	};

	class Sampler
	{
	public:
		SamplerHandle GetHandle() { return m_Sampler; }
	private:
		SamplerHandle m_Sampler;

		friend class ResourceAllocator;
	};
}
#pragma once
#include "RadianceEngine/Graphics/Handle.h"
#include "RadianceEngine/Graphics/Common.h"
#include <optional>

namespace Rdn
{
	struct SamplerDesc
	{
		Filter MinFilter = Filter::Linear;
		Filter MagFilter = Filter::Linear;
		SamplerAddressMode AddressMode = SamplerAddressMode::MirroredRepeat;
		std::optional<SamplerAddressMode> AddressModeV;
		float MaxAnisotropy = 1.0f;
	};

	class Sampler : NonCopyable
	{
	public:
		SamplerHandle GetHandle() const { return m_Sampler; }

	private:
		SamplerHandle m_Sampler{};

		friend class ResourceAllocator;
	};
}

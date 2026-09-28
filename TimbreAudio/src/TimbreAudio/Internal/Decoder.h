#pragma once
#include "TimbreAudio/FileSystem.h"
#include "TimbreAudio/Internal/SoundAsset.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Timbre::Internal
{
	// Decodes an audio file to interleaved float frames with 1 or 2 channels.
	class Decoder
	{
	public:
		virtual ~Decoder() = default;

		const AudioFormat& GetFormat() const { return m_Format; }

		// Reads up to frames frames. Returns fewer only at the end of the file or on an error.
		virtual uint64_t Read(float* output, uint64_t frames) = 0;
		virtual bool Seek(uint64_t frame) = 0;

	protected:
		AudioFormat m_Format;
	};

	// Detects the format from the file contents. Returns null and sets error on failure.
	std::unique_ptr<Decoder> OpenDecoder(std::unique_ptr<AudioFile> file, std::string& error);

	// The exact inverse of the mixer's int16 to float conversion (divide by 32768), so 16 bit
	// sources survive the round trip unchanged.
	inline int16_t FloatToPcm16(float sample)
	{
		return int16_t(std::clamp(std::lrint(sample * 32768.0f), -32768L, 32767L));
	}

	// Decodes everything that's left into 16 bit samples.
	void DecodeToPcm16(Decoder& decoder, std::vector<int16_t>& pcm);

	std::unique_ptr<AudioFile> OpenMemoryFile(std::shared_ptr<const std::vector<uint8_t>> data);
	// Reads the rest of the file into memory.
	std::shared_ptr<const std::vector<uint8_t>> ReadAll(AudioFile& file);
}

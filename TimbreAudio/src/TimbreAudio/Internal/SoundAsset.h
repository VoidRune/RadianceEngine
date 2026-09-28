#pragma once
#include "TimbreAudio/Sound.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Timbre::Internal
{
	class EngineImpl;

	// Rounds, so times given as floats like 0.9f land on the frame the caller meant.
	inline uint64_t SecondsToFrames(double seconds, uint32_t sampleRate)
	{
		return uint64_t(std::llround(std::max(0.0, seconds) * sampleRate));
	}

	struct AudioFormat
	{
		// 1 or 2, sounds with more channels are downmixed to stereo when decoding.
		uint32_t Channels = 0;
		uint32_t SampleRate = 0;
		// 0 when unknown, which can only happen for streams.
		uint64_t FrameCount = 0;
		// Loop region from the file's tags; LoopEnd is 0 when there is none.
		uint64_t LoopStart = 0;
		uint64_t LoopEnd = 0;
	};

	// Audio data shared by every Sound loaded from the same file. Everything except State is
	// written before State becomes Ready and is read-only afterwards.
	struct SoundAsset
	{
		std::string Path;
		LoadMode Mode = LoadMode::Decompress;
		std::atomic<LoadState> State = LoadState::Loading;
		AudioFormat Format;
		// Decompress mode: interleaved 16 bit samples.
		std::vector<int16_t> Pcm;
		// Compressed mode, or streams loaded from memory: the encoded file.
		std::shared_ptr<const std::vector<uint8_t>> FileData;

		uint64_t GetMemoryBytes() const { return Pcm.size() * sizeof(int16_t) + (FileData ? FileData->size() : 0); }
	};

	struct SoundData
	{
		std::shared_ptr<SoundAsset> Asset;
		SoundDesc Desc;
		EngineImpl* Engine = nullptr;
		// Voice slots playing this sound. Game side only, guarded by the engine mutex.
		std::vector<uint32_t> Voices;

		// Loop region in frames. Only valid once the asset is ready. The end is UINT64_MAX when the
		// length of a stream is unknown.
		void GetLoopRegion(uint64_t& start, uint64_t& end) const
		{
			const AudioFormat& format = Asset->Format;
			const uint64_t length = format.FrameCount > 0 ? format.FrameCount : UINT64_MAX;
			start = 0;
			end = length;
			if (format.LoopEnd > format.LoopStart)
			{
				start = format.LoopStart;
				end = format.LoopEnd;
			}
			if (Desc.LoopStart >= 0.0f)
				start = SecondsToFrames(Desc.LoopStart, format.SampleRate);
			if (Desc.LoopEnd > 0.0f)
				end = SecondsToFrames(Desc.LoopEnd, format.SampleRate);
			end = std::min(end, length);
			if (start >= end)
			{
				start = 0;
				end = length;
			}
		}
	};
}

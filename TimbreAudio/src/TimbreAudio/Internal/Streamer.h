#pragma once
#include "TimbreAudio/FileSystem.h"
#include "TimbreAudio/Internal/Decoder.h"
#include "TimbreAudio/Internal/SoundAsset.h"
#include "TimbreAudio/Internal/SpscQueue.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <semaphore>
#include <thread>
#include <vector>

namespace Timbre::Internal
{
	struct StreamMarker
	{
		enum class Kind : uint8_t
		{
			// Data from RingFrame on plays linearly from SourceFrame.
			Segment,
			// Data from RingFrame on repeats the loop region [SourceFrame, SourceFrame + LoopLength).
			// Sent once when looping starts, not for every pass, so short loops can't fill the queue.
			Loop,
			// Answer to seek request Serial: data from RingFrame on starts at SourceFrame, everything
			// before it is stale.
			Flush,
			// The stream ends at RingFrame.
			End,
		};

		Kind Type = Kind::Segment;
		uint32_t Serial = 0;
		uint64_t RingFrame = 0;
		uint64_t SourceFrame = 0;
		uint64_t LoopLength = 0;
	};

	enum class StreamStatus : uint8_t
	{
		Opening,
		Ready,
		Failed,
	};

	// One playing instance of a streamed sound. The streaming thread decodes into a ring buffer that
	// the mixer reads. Ring frame indices only grow; their low bits select the slot in the ring.
	class StreamInstance
	{
	public:
		StreamInstance(std::shared_ptr<SoundData> sound, double startTime, bool looping);

		const SoundData& GetSound() const { return *m_Sound; }

		// Written by the mixer, read by the streamer
		std::atomic<bool> Looping;
		std::atomic<double> SeekTime = 0.0;
		std::atomic<uint32_t> SeekSerial = 0;
		std::atomic<uint64_t> ReadFrame = 0;

		// Written by the streamer, read by the mixer
		std::atomic<StreamStatus> Status = StreamStatus::Opening;
		std::atomic<uint64_t> WriteFrame = 0;
		SpscQueue<StreamMarker> Markers{ 32 };

		// Set before Status becomes Ready and constant afterwards
		uint32_t Channels = 0;
		uint32_t SampleRate = 0;
		uint64_t Capacity = 0;
		std::vector<int16_t> Ring;

	private:
		friend class Streamer;

		std::shared_ptr<SoundData> m_Sound;
		double m_StartTime = 0.0;
		std::unique_ptr<Decoder> m_Decoder;
		uint64_t m_DecodeFrame = 0;
		uint64_t m_LoopStart = 0;
		uint64_t m_LoopEnd = UINT64_MAX;
		uint32_t m_HandledSeek = 0;
		uint64_t m_FlushFrame = 0;
		bool m_FlushPending = false;
		// A Loop marker was sent and the decoder keeps wrapping.
		bool m_Looped = false;
		bool m_AtEnd = false;
		bool m_EndSent = false;
	};

	class Streamer
	{
	public:
		Streamer(std::shared_ptr<AudioFileSystem> fileSystem, float bufferSeconds, bool threaded);
		~Streamer();
		Streamer(const Streamer&) = delete;
		Streamer& operator=(const Streamer&) = delete;

		void Add(std::shared_ptr<StreamInstance> stream);
		void Remove(StreamInstance* stream);
		// Asks the streaming thread to run soon. Real-time safe.
		void Wake();
		// Services every stream once. Runs on the streaming thread, or on the render thread when not threaded.
		void Pump();

	private:
		void Run();
		void Open(StreamInstance& stream);
		void Service(StreamInstance& stream);

		std::shared_ptr<AudioFileSystem> m_FileSystem;
		float m_BufferSeconds;
		bool m_Threaded;
		std::vector<std::shared_ptr<StreamInstance>> m_Streams;
		std::vector<float> m_Scratch;

		std::mutex m_Mutex;
		std::vector<std::shared_ptr<StreamInstance>> m_Added;
		std::vector<StreamInstance*> m_Removed;

		std::counting_semaphore<> m_Semaphore{ 0 };
		std::atomic<bool> m_WakePending = false;
		std::atomic<bool> m_Running = true;
		std::thread m_Thread;
	};
}

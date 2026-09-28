#include "Streamer.h"
#include "Log.h"
#include <algorithm>
#include <bit>
#include <chrono>

namespace Timbre::Internal
{
	namespace
	{
		constexpr uint64_t MinimumCapacity = 16384;
		constexpr uint64_t MinimumFittedCapacity = 4096;
		constexpr uint64_t ChunkFrames = 4096;
	}

	StreamInstance::StreamInstance(std::shared_ptr<SoundData> sound, double startTime, bool looping)
		: Looping(looping), m_Sound(std::move(sound)), m_StartTime(startTime)
	{
	}

	Streamer::Streamer(std::shared_ptr<AudioFileSystem> fileSystem, float bufferSeconds, bool threaded)
		: m_FileSystem(std::move(fileSystem)), m_BufferSeconds(bufferSeconds), m_Threaded(threaded), m_Scratch(ChunkFrames * 2)
	{
		if (m_Threaded)
			m_Thread = std::thread([this] { Run(); });
	}

	Streamer::~Streamer()
	{
		if (m_Thread.joinable())
		{
			m_Running.store(false, std::memory_order_release);
			m_Semaphore.release();
			m_Thread.join();
		}
	}

	void Streamer::Add(std::shared_ptr<StreamInstance> stream)
	{
		{
			std::scoped_lock lock(m_Mutex);
			m_Added.push_back(std::move(stream));
		}
		Wake();
	}

	void Streamer::Remove(StreamInstance* stream)
	{
		{
			std::scoped_lock lock(m_Mutex);
			m_Removed.push_back(stream);
		}
		Wake();
	}

	void Streamer::Wake()
	{
		if (m_Threaded && !m_WakePending.exchange(true, std::memory_order_acq_rel))
			m_Semaphore.release();
	}

	void Streamer::Run()
	{
		while (m_Running.load(std::memory_order_acquire))
		{
			m_WakePending.store(false, std::memory_order_release);
			Pump();
			(void)m_Semaphore.try_acquire_for(std::chrono::milliseconds(5));
		}
	}

	void Streamer::Pump()
	{
		std::vector<std::shared_ptr<StreamInstance>> removed;
		{
			std::scoped_lock lock(m_Mutex);
			for (std::shared_ptr<StreamInstance>& stream : m_Added)
				m_Streams.push_back(std::move(stream));
			m_Added.clear();
			for (StreamInstance* stream : m_Removed)
			{
				const auto it = std::find_if(m_Streams.begin(), m_Streams.end(), [&](const auto& s) { return s.get() == stream; });
				if (it != m_Streams.end())
				{
					removed.push_back(std::move(*it));
					*it = std::move(m_Streams.back());
					m_Streams.pop_back();
				}
			}
			m_Removed.clear();
		}
		// Streams are destroyed out here so closing files doesn't block Add() and Remove().
		removed.clear();

		for (const std::shared_ptr<StreamInstance>& stream : m_Streams)
			Service(*stream);
	}

	void Streamer::Open(StreamInstance& stream)
	{
		const SoundData& sound = *stream.m_Sound;
		const SoundAsset& asset = *sound.Asset;
		const LoadState state = asset.State.load(std::memory_order_acquire);
		if (state == LoadState::Loading)
			return;
		if (state == LoadState::Failed)
		{
			stream.Status.store(StreamStatus::Failed, std::memory_order_release);
			return;
		}

		std::unique_ptr<AudioFile> file = asset.FileData ? OpenMemoryFile(asset.FileData) : m_FileSystem->Open(asset.Path);
		std::string error = "can't open the file";
		std::unique_ptr<Decoder> decoder = file ? OpenDecoder(std::move(file), error) : nullptr;
		if (!decoder)
		{
			TIMBRE_LOG_ERROR("Failed to stream {}: {}", asset.Path, error);
			stream.Status.store(StreamStatus::Failed, std::memory_order_release);
			return;
		}

		const AudioFormat& format = decoder->GetFormat();
		stream.Channels = format.Channels;
		stream.SampleRate = format.SampleRate;
		const uint64_t target = uint64_t(double(m_BufferSeconds) * format.SampleRate);
		const bool fits = format.FrameCount > 0 && format.FrameCount + 8 <= target && !stream.Looping.load(std::memory_order_relaxed);
		stream.Capacity = fits ? std::bit_ceil(std::max(MinimumFittedCapacity, format.FrameCount + 8)) : std::bit_ceil(std::max(MinimumCapacity, target));
		stream.Ring.assign(stream.Capacity * format.Channels, 0);
		sound.GetLoopRegion(stream.m_LoopStart, stream.m_LoopEnd);

		uint64_t start = SecondsToFrames(stream.m_StartTime, format.SampleRate);
		if (format.FrameCount > 0)
			start = std::min(start, format.FrameCount);
		if (start > 0 && !decoder->Seek(start))
		{
			TIMBRE_LOG_WARNING("Failed to seek {} to frame {}", asset.Path, start);
			start = 0;
			decoder->Seek(0);
		}
		stream.m_DecodeFrame = start;
		stream.m_Decoder = std::move(decoder);
		stream.Markers.PushAndPublish({ StreamMarker::Kind::Segment, 0, 0, start });
		stream.Status.store(StreamStatus::Ready, std::memory_order_release);
	}

	void Streamer::Service(StreamInstance& stream)
	{
		if (stream.Status.load(std::memory_order_relaxed) == StreamStatus::Opening)
			Open(stream);
		if (stream.Status.load(std::memory_order_relaxed) != StreamStatus::Ready)
			return;

		Decoder& decoder = *stream.m_Decoder;
		const uint64_t length = stream.m_Sound->Asset->Format.FrameCount;

		const uint32_t serial = stream.SeekSerial.load(std::memory_order_acquire);
		if (serial != stream.m_HandledSeek)
		{
			stream.m_HandledSeek = serial;
			uint64_t frame = SecondsToFrames(stream.SeekTime.load(std::memory_order_relaxed), stream.SampleRate);
			if (length > 0)
				frame = std::min(frame, length);
			stream.m_AtEnd = false;
			stream.m_EndSent = false;
			if (!decoder.Seek(frame))
			{
				TIMBRE_LOG_WARNING("Failed to seek {} to frame {}", stream.m_Sound->Asset->Path, frame);
				stream.m_AtEnd = true;
			}
			stream.m_DecodeFrame = frame;
			stream.m_FlushFrame = frame;
			stream.m_FlushPending = true;
			stream.m_Looped = false;
		}

		uint64_t write = stream.WriteFrame.load(std::memory_order_relaxed);
		if (stream.m_FlushPending)
		{
			if (!stream.Markers.PushAndPublish({ StreamMarker::Kind::Flush, stream.m_HandledSeek, write, stream.m_FlushFrame }))
				return;
			stream.m_FlushPending = false;
		}

		const uint64_t mask = stream.Capacity - 1;
		while (!stream.m_AtEnd)
		{
			const uint64_t free = stream.Capacity - (write - stream.ReadFrame.load(std::memory_order_acquire));
			if (free < std::min(ChunkFrames, stream.Capacity / 4))
				break;

			// While looping, and while leaving a loop, reads stop at the loop end so the loop point
			// lands exactly on a marker. A stream started past the loop end plays to the end.
			const bool looping = stream.Looping.load(std::memory_order_relaxed);
			const bool bounded = (looping || stream.m_Looped) && stream.m_DecodeFrame <= stream.m_LoopEnd;
			if (bounded && stream.m_DecodeFrame == stream.m_LoopEnd)
			{
				if (!looping)
				{
					// Looping was switched off, the data continues linearly after the loop end.
					if (!stream.Markers.PushAndPublish({ StreamMarker::Kind::Segment, 0, write, stream.m_LoopEnd }))
						break;
					stream.m_Looped = false;
					continue;
				}
				if (!stream.m_Looped)
				{
					if (!stream.Markers.PushAndPublish({ StreamMarker::Kind::Loop, 0, write, stream.m_LoopStart, stream.m_LoopEnd - stream.m_LoopStart }))
						break;
					stream.m_Looped = true;
				}
				if (!decoder.Seek(stream.m_LoopStart))
				{
					TIMBRE_LOG_WARNING("Failed to loop {}", stream.m_Sound->Asset->Path);
					stream.m_AtEnd = true;
					break;
				}
				stream.m_DecodeFrame = stream.m_LoopStart;
				continue;
			}

			const uint64_t offset = write & mask;
			uint64_t count = std::min({ free, stream.Capacity - offset, ChunkFrames });
			if (bounded)
				count = std::min(count, stream.m_LoopEnd - stream.m_DecodeFrame);
			const uint64_t decoded = decoder.Read(m_Scratch.data(), count);
			int16_t* destination = stream.Ring.data() + offset * stream.Channels;
			for (size_t i = 0; i < size_t(decoded) * stream.Channels; i++)
				destination[i] = FloatToPcm16(m_Scratch[i]);
			stream.m_DecodeFrame += decoded;
			write += decoded;
			stream.WriteFrame.store(write, std::memory_order_release);

			if (decoded < count)
			{
				// The file ended before the loop end (unknown length or a wrong tag), so it loops from
				// here. This can only happen on the first pass, before the Loop marker is sent.
				if (looping && bounded && stream.m_DecodeFrame > stream.m_LoopStart)
					stream.m_LoopEnd = stream.m_DecodeFrame;
				else
					stream.m_AtEnd = true;
			}
		}

		if (stream.m_AtEnd && !stream.m_EndSent)
			stream.m_EndSent = stream.Markers.PushAndPublish({ StreamMarker::Kind::End, 0, write, stream.m_DecodeFrame });
	}
}

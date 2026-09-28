#include "Decoder.h"
#include "Miniaudio.h"

#define OV_EXCLUDE_STATIC_CALLBACKS
#include <minivorbis/minivorbis.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <format>
#include <string_view>

namespace Timbre::Internal
{
	namespace
	{
		class MemoryFile final : public AudioFile
		{
		public:
			explicit MemoryFile(std::shared_ptr<const std::vector<uint8_t>> data) : m_Data(std::move(data)) {}

			size_t Read(void* buffer, size_t bytes) override
			{
				const size_t count = std::min(bytes, m_Data->size() - m_Position);
				std::memcpy(buffer, m_Data->data() + m_Position, count);
				m_Position += count;
				return count;
			}

			bool Seek(int64_t offset, SeekOrigin origin) override
			{
				const int64_t base = origin == SeekOrigin::Begin ? 0 : origin == SeekOrigin::Current ? int64_t(m_Position) : int64_t(m_Data->size());
				const int64_t position = base + offset;
				if (position < 0 || position > int64_t(m_Data->size()))
					return false;
				m_Position = size_t(position);
				return true;
			}

			int64_t Tell() const override { return int64_t(m_Position); }
			int64_t Size() const override { return int64_t(m_Data->size()); }

		private:
			std::shared_ptr<const std::vector<uint8_t>> m_Data;
			size_t m_Position = 0;
		};

		// Folds any channel count to mono or stereo. Frames with more than two channels use the
		// Vorbis channel order: L, C, R, then surrounds (and LFE last, which is dropped).
		void DownmixToStereo(const float* const* planar, uint32_t channels, uint32_t frames, float* output)
		{
			for (uint32_t i = 0; i < frames; i++)
			{
				float left = 0.0f;
				float right = 0.0f;
				switch (channels)
				{
				case 3:
					left = planar[0][i] + 0.7071f * planar[1][i];
					right = planar[2][i] + 0.7071f * planar[1][i];
					break;
				case 4:
					left = planar[0][i] + 0.7071f * planar[2][i];
					right = planar[1][i] + 0.7071f * planar[3][i];
					break;
				default:
					left = planar[0][i] + 0.7071f * planar[1][i] + 0.7071f * planar[3][i];
					right = planar[2][i] + 0.7071f * planar[1][i] + 0.7071f * planar[4][i];
					if (channels >= 7)
					{
						left += 0.7071f * planar[5][i];
						right += 0.7071f * planar[channels == 7 ? 5 : 6][i];
					}
					break;
				}
				output[i * 2 + 0] = left * 0.5f;
				output[i * 2 + 1] = right * 0.5f;
			}
		}

		bool EqualsIgnoreCase(std::string_view a, std::string_view b)
		{
			return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return std::toupper(uint8_t(x)) == std::toupper(uint8_t(y)); });
		}

		class VorbisDecoder final : public Decoder
		{
		public:
			~VorbisDecoder() override
			{
				if (m_Open)
					ov_clear(&m_Vorbis);
			}

			bool Open(std::unique_ptr<AudioFile> file, std::string& error)
			{
				m_File = std::move(file);
				const ov_callbacks callbacks = { &ReadCallback, &SeekCallback, nullptr, &TellCallback };
				const int result = ov_open_callbacks(m_File.get(), &m_Vorbis, nullptr, 0, callbacks);
				if (result != 0)
				{
					error = result == OV_ENOTVORBIS ? "the Ogg file doesn't contain Vorbis audio" : std::format("invalid Ogg Vorbis file (error {})", result);
					return false;
				}
				m_Open = true;

				const vorbis_info* info = ov_info(&m_Vorbis, -1);
				if (info == nullptr || info->channels < 1 || info->rate <= 0)
				{
					error = "invalid Vorbis stream";
					return false;
				}
				m_Format.Channels = std::min(uint32_t(info->channels), 2u);
				m_Format.SampleRate = uint32_t(info->rate);
				const ogg_int64_t total = ov_pcm_total(&m_Vorbis, -1);
				m_Format.FrameCount = total > 0 ? uint64_t(total) : 0;
				ReadLoopTags();
				return true;
			}

			uint64_t Read(float* output, uint64_t frames) override
			{
				uint64_t done = 0;
				int holes = 0;
				while (done < frames)
				{
					float** pcm = nullptr;
					int section = 0;
					const long count = ov_read_float(&m_Vorbis, &pcm, int(std::min<uint64_t>(frames - done, 4096)), &section);
					if (count == OV_HOLE && ++holes < 64)
						continue;
					if (count <= 0)
						break;

					const vorbis_info* info = ov_info(&m_Vorbis, section);
					const uint32_t channels = info != nullptr ? uint32_t(info->channels) : m_Format.Channels;
					float* destination = output + done * m_Format.Channels;
					if (channels == m_Format.Channels)
					{
						for (long i = 0; i < count; i++)
							for (uint32_t c = 0; c < channels; c++)
								destination[i * channels + c] = pcm[c][i];
					}
					else if (m_Format.Channels == 1)
					{
						// A chained stream switched layouts, fold to mono.
						for (long i = 0; i < count; i++)
						{
							float sum = 0.0f;
							for (uint32_t c = 0; c < channels; c++)
								sum += pcm[c][i];
							destination[i] = sum / float(channels);
						}
					}
					else if (channels == 1)
					{
						for (long i = 0; i < count; i++)
							destination[i * 2] = destination[i * 2 + 1] = pcm[0][i];
					}
					else
						DownmixToStereo(pcm, channels, uint32_t(count), destination);
					done += uint64_t(count);
				}
				return done;
			}

			bool Seek(uint64_t frame) override { return ov_pcm_seek(&m_Vorbis, ogg_int64_t(frame)) == 0; }

		private:
			static size_t ReadCallback(void* buffer, size_t size, size_t count, void* source)
			{
				if (size == 0)
					return 0;
				return static_cast<AudioFile*>(source)->Read(buffer, size * count) / size;
			}

			static int SeekCallback(void* source, ogg_int64_t offset, int whence)
			{
				const SeekOrigin origin = whence == SEEK_SET ? SeekOrigin::Begin : whence == SEEK_CUR ? SeekOrigin::Current : SeekOrigin::End;
				return static_cast<AudioFile*>(source)->Seek(int64_t(offset), origin) ? 0 : -1;
			}

			static long TellCallback(void* source)
			{
				return long(static_cast<AudioFile*>(source)->Tell());
			}

			// LOOPSTART with LOOPLENGTH or LOOPEND, in samples. Used by many games and tools.
			void ReadLoopTags()
			{
				const vorbis_comment* comment = ov_comment(&m_Vorbis, -1);
				if (comment == nullptr)
					return;

				uint64_t start = 0;
				uint64_t length = 0;
				uint64_t end = 0;
				bool hasStart = false;
				for (int i = 0; i < comment->comments; i++)
				{
					const std::string_view text(comment->user_comments[i], size_t(comment->comment_lengths[i]));
					const size_t separator = text.find('=');
					if (separator == std::string_view::npos)
						continue;
					const std::string_view key = text.substr(0, separator);
					const std::string_view value = text.substr(separator + 1);
					uint64_t number = 0;
					if (std::from_chars(value.data(), value.data() + value.size(), number).ec != std::errc())
						continue;
					if (EqualsIgnoreCase(key, "LOOPSTART") || EqualsIgnoreCase(key, "LOOP_START"))
					{
						start = number;
						hasStart = true;
					}
					else if (EqualsIgnoreCase(key, "LOOPLENGTH") || EqualsIgnoreCase(key, "LOOP_LENGTH"))
						length = number;
					else if (EqualsIgnoreCase(key, "LOOPEND") || EqualsIgnoreCase(key, "LOOP_END"))
						end = number;
				}

				if (!hasStart)
					return;
				if (end == 0)
					end = length > 0 ? start + length : m_Format.FrameCount;
				if (end > start)
				{
					m_Format.LoopStart = start;
					m_Format.LoopEnd = end;
				}
			}

			std::unique_ptr<AudioFile> m_File;
			OggVorbis_File m_Vorbis{};
			bool m_Open = false;
		};

		// WAV, FLAC and MP3 through miniaudio's decoders.
		class MiniaudioDecoder final : public Decoder
		{
		public:
			~MiniaudioDecoder() override
			{
				if (m_Open)
					ma_decoder_uninit(&m_Decoder);
			}

			bool Open(std::unique_ptr<AudioFile> file, std::string& error)
			{
				m_File = std::move(file);
				if (!Initialize(0))
				{
					error = "unsupported or invalid audio file";
					return false;
				}

				ma_format format = ma_format_unknown;
				ma_uint32 channels = 0;
				ma_uint32 sampleRate = 0;
				ma_decoder_get_data_format(&m_Decoder, &format, &channels, &sampleRate, nullptr, 0);
				if (channels > 2)
				{
					// Let miniaudio's channel mapping fold surround files to stereo.
					ma_decoder_uninit(&m_Decoder);
					m_Open = false;
					if (!m_File->Seek(0, SeekOrigin::Begin) || !Initialize(2))
					{
						error = "failed to downmix the audio file to stereo";
						return false;
					}
					channels = 2;
				}
				if (channels == 0 || sampleRate == 0)
				{
					error = "invalid audio format";
					return false;
				}

				m_Format.Channels = channels;
				m_Format.SampleRate = sampleRate;
				ma_uint64 length = 0;
				if (ma_decoder_get_length_in_pcm_frames(&m_Decoder, &length) == MA_SUCCESS)
					m_Format.FrameCount = length;
				return true;
			}

			uint64_t Read(float* output, uint64_t frames) override
			{
				uint64_t done = 0;
				while (done < frames)
				{
					ma_uint64 count = 0;
					const ma_result result = ma_decoder_read_pcm_frames(&m_Decoder, output + done * m_Format.Channels, frames - done, &count);
					done += count;
					if (result != MA_SUCCESS || count == 0)
						break;
				}
				return done;
			}

			bool Seek(uint64_t frame) override { return ma_decoder_seek_to_pcm_frame(&m_Decoder, frame) == MA_SUCCESS; }

		private:
			bool Initialize(uint32_t channels)
			{
				const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, channels, 0);
				m_Open = ma_decoder_init(&ReadCallback, &SeekCallback, this, &config, &m_Decoder) == MA_SUCCESS;
				return m_Open;
			}

			static ma_result ReadCallback(ma_decoder* decoder, void* buffer, size_t bytes, size_t* bytesRead)
			{
				const size_t count = static_cast<MiniaudioDecoder*>(decoder->pUserData)->m_File->Read(buffer, bytes);
				if (bytesRead != nullptr)
					*bytesRead = count;
				return count == 0 && bytes > 0 ? MA_AT_END : MA_SUCCESS;
			}

			static ma_result SeekCallback(ma_decoder* decoder, ma_int64 offset, ma_seek_origin origin)
			{
				const SeekOrigin seekOrigin = origin == ma_seek_origin_start ? SeekOrigin::Begin : origin == ma_seek_origin_current ? SeekOrigin::Current : SeekOrigin::End;
				return static_cast<MiniaudioDecoder*>(decoder->pUserData)->m_File->Seek(offset, seekOrigin) ? MA_SUCCESS : MA_BAD_SEEK;
			}

			std::unique_ptr<AudioFile> m_File;
			ma_decoder m_Decoder{};
			bool m_Open = false;
		};
	}

	std::unique_ptr<Decoder> OpenDecoder(std::unique_ptr<AudioFile> file, std::string& error)
	{
		if (!file)
		{
			error = "no file";
			return nullptr;
		}

		char magic[4] = {};
		const size_t magicSize = file->Read(magic, sizeof(magic));
		if (!file->Seek(0, SeekOrigin::Begin))
		{
			error = "the file is not seekable";
			return nullptr;
		}

		if (magicSize == 4 && std::memcmp(magic, "OggS", 4) == 0)
		{
			auto decoder = std::make_unique<VorbisDecoder>();
			if (!decoder->Open(std::move(file), error))
				return nullptr;
			return decoder;
		}

		auto decoder = std::make_unique<MiniaudioDecoder>();
		if (!decoder->Open(std::move(file), error))
			return nullptr;
		return decoder;
	}

	void DecodeToPcm16(Decoder& decoder, std::vector<int16_t>& pcm)
	{
		constexpr uint64_t ChunkFrames = 4096;
		const AudioFormat& format = decoder.GetFormat();
		std::vector<float> chunk(ChunkFrames * format.Channels);
		if (format.FrameCount > 0)
			pcm.reserve(format.FrameCount * format.Channels);

		while (true)
		{
			const uint64_t frames = decoder.Read(chunk.data(), ChunkFrames);
			const size_t samples = size_t(frames * format.Channels);
			const size_t offset = pcm.size();
			pcm.resize(offset + samples);
			for (size_t i = 0; i < samples; i++)
				pcm[offset + i] = FloatToPcm16(chunk[i]);
			if (frames < ChunkFrames)
				break;
		}
		pcm.shrink_to_fit();
	}

	std::unique_ptr<AudioFile> OpenMemoryFile(std::shared_ptr<const std::vector<uint8_t>> data)
	{
		return std::make_unique<MemoryFile>(std::move(data));
	}

	std::shared_ptr<const std::vector<uint8_t>> ReadAll(AudioFile& file)
	{
		auto data = std::make_shared<std::vector<uint8_t>>();
		const int64_t size = file.Size() - file.Tell();
		if (size > 0)
		{
			data->resize(size_t(size));
			data->resize(file.Read(data->data(), data->size()));
		}
		else
		{
			uint8_t chunk[65536];
			while (const size_t count = file.Read(chunk, sizeof(chunk)))
				data->insert(data->end(), chunk, chunk + count);
		}
		return data;
	}
}

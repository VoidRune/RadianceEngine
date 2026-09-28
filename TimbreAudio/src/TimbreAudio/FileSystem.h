#pragma once
#include <cstdint>
#include <memory>
#include <string>

namespace Timbre
{
	enum class SeekOrigin
	{
		Begin,
		Current,
		End,
	};

	// A readable file. Each file is used by one thread at a time.
	class AudioFile
	{
	public:
		virtual ~AudioFile() = default;

		virtual size_t Read(void* buffer, size_t bytes) = 0;
		virtual bool Seek(int64_t offset, SeekOrigin origin) = 0;
		virtual int64_t Tell() const = 0;
		virtual int64_t Size() const = 0;
	};

	// Opens files by path. Implement it to load audio from pak files or a virtual file system.
	// Open() is called from the game, loading and streaming threads, so it must be thread-safe.
	class AudioFileSystem
	{
	public:
		virtual ~AudioFileSystem() = default;

		virtual std::unique_ptr<AudioFile> Open(const std::string& path) = 0;
	};

	// Opens files from disk. Paths are UTF-8.
	std::shared_ptr<AudioFileSystem> CreateDiskFileSystem();
}

#include "TimbreAudio/FileSystem.h"
#include <cstdio>
#include <filesystem>

namespace Timbre
{
	namespace
	{
		class DiskFile final : public AudioFile
		{
		public:
			explicit DiskFile(FILE* file)
				: m_File(file)
			{
				if (Seek(0, SeekOrigin::End))
					m_Size = Tell();
				Seek(0, SeekOrigin::Begin);
			}

			~DiskFile() override { std::fclose(m_File); }

			size_t Read(void* buffer, size_t bytes) override { return std::fread(buffer, 1, bytes, m_File); }

			bool Seek(int64_t offset, SeekOrigin origin) override
			{
				const int whence = origin == SeekOrigin::Begin ? SEEK_SET : origin == SeekOrigin::Current ? SEEK_CUR : SEEK_END;
#ifdef _WIN32
				return _fseeki64(m_File, offset, whence) == 0;
#else
				return fseeko(m_File, off_t(offset), whence) == 0;
#endif
			}

			int64_t Tell() const override
			{
#ifdef _WIN32
				return _ftelli64(m_File);
#else
				return int64_t(ftello(m_File));
#endif
			}

			int64_t Size() const override { return m_Size; }

		private:
			FILE* m_File;
			int64_t m_Size = 0;
		};

		class DiskFileSystem final : public AudioFileSystem
		{
		public:
			std::unique_ptr<AudioFile> Open(const std::string& path) override
			{
				// Going through a u8string makes UTF-8 paths work on Windows too.
				const std::filesystem::path filePath(std::u8string(path.begin(), path.end()));
#ifdef _WIN32
				FILE* file = _wfopen(filePath.c_str(), L"rb");
#else
				FILE* file = std::fopen(filePath.c_str(), "rb");
#endif
				if (file == nullptr)
					return nullptr;
				return std::make_unique<DiskFile>(file);
			}
		};
	}

	std::shared_ptr<AudioFileSystem> CreateDiskFileSystem()
	{
		return std::make_shared<DiskFileSystem>();
	}
}

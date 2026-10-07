#include "VectorAtlas.h"
#include <array>
#include <filesystem>
#include <fstream>

namespace Game
{
	namespace
	{
		constexpr uint32_t FileMagic = 0x54414955;
		constexpr uint32_t FileVersion = 5;
		constexpr uint64_t HashSeed = 0xCBF29CE484222325ull;

		uint64_t Hash(uint64_t hash, const void* data, size_t size)
		{
			const uint8_t* bytes = static_cast<const uint8_t*>(data);
			for (size_t i = 0; i < size; i++)
				hash = (hash ^ bytes[i]) * 0x100000001B3ull;
			return hash;
		}

		template<typename T>
		uint64_t HashValue(uint64_t hash, const T& value)
		{
			return Hash(hash, &value, sizeof(value));
		}

		uint64_t HashString(uint64_t hash, const std::string& text)
		{
			return Hash(HashValue(hash, uint64_t(text.size())), text.data(), text.size());
		}

		uint64_t HashFileContents(uint64_t hash, const std::string& path)
		{
			hash = HashString(hash, path);
			std::ifstream file(path, std::ios::binary);
			std::array<char, 65536> buffer;
			while (file)
			{
				file.read(buffer.data(), std::streamsize(buffer.size()));
				hash = Hash(hash, buffer.data(), size_t(file.gcount()));
			}
			return hash;
		}

		template<typename T>
		void Write(std::ofstream& file, const T& value)
		{
			file.write(reinterpret_cast<const char*>(&value), sizeof(value));
		}

		template<typename T>
		bool Read(std::ifstream& file, T& value)
		{
			return bool(file.read(reinterpret_cast<char*>(&value), sizeof(value)));
		}
	}

	const AtlasGlyph* FontFace::Find(uint32_t codepoint) const
	{
		const auto it = Glyphs.find(codepoint);
		if (it != Glyphs.end())
			return &it->second;
		const auto fallback = Glyphs.find('?');
		return fallback != Glyphs.end() ? &fallback->second : nullptr;
	}

	float FontFace::GetKerning(uint32_t left, uint32_t right) const
	{
		if (Kerning.empty())
			return 0.0f;
		const auto it = Kerning.find(KerningPair(left, right));
		return it != Kerning.end() ? it->second : 0.0f;
	}

	uint64_t HashAtlasLayout(const VectorAtlasDesc& desc)
	{
		uint64_t hash = HashValue(HashSeed, FileVersion);
		hash = HashValue(hash, desc.GlyphSize);
		hash = HashValue(hash, desc.IconSize);
		hash = HashValue(hash, desc.DistanceRange);
		hash = HashValue(hash, desc.Width);
		for (const FontSource& font : desc.Fonts)
		{
			hash = HashString(hash, font.Path);
			for (const auto& range : font.Ranges)
				hash = HashValue(hash, range);
		}
		for (const std::string& icon : desc.Icons)
			hash = HashString(hash, icon);
		return hash;
	}

	uint64_t HashAtlasSources(const VectorAtlasDesc& desc)
	{
		uint64_t hash = HashSeed;
		for (const FontSource& font : desc.Fonts)
			hash = HashFileContents(hash, font.Path);
		for (const std::string& icon : desc.Icons)
			hash = HashFileContents(hash, icon);
		return hash;
	}

	bool SaveVectorAtlas(const std::string& path, const VectorAtlas& atlas)
	{
		std::error_code error;
		std::filesystem::create_directories(std::filesystem::path(path).parent_path(), error);
		const std::string temporary = path + ".tmp";
		{
			std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
			if (!file)
				return false;
			Write(file, FileMagic);
			Write(file, FileVersion);
			Write(file, atlas.LayoutHash);
			Write(file, atlas.SourceHash);
			Write(file, atlas.Width);
			Write(file, atlas.Height);
			Write(file, atlas.DistanceRange);
			Write(file, uint32_t(atlas.Fonts.size()));
			for (const FontFace& font : atlas.Fonts)
			{
				Write(file, font.Ascender);
				Write(file, font.Descender);
				Write(file, font.LineGap);
				Write(file, uint32_t(font.Glyphs.size()));
				for (const auto& [codepoint, glyph] : font.Glyphs)
				{
					Write(file, codepoint);
					Write(file, glyph);
				}
				Write(file, uint32_t(font.Kerning.size()));
				for (const auto& [pair, amount] : font.Kerning)
				{
					Write(file, pair);
					Write(file, amount);
				}
			}
			Write(file, uint32_t(atlas.Icons.size()));
			for (const AtlasIcon& icon : atlas.Icons)
				Write(file, icon);
			file.write(reinterpret_cast<const char*>(atlas.Pixels.data()), std::streamsize(atlas.Pixels.size()));
			if (!file)
				return false;
		}
		std::filesystem::rename(temporary, path, error);
		return !error;
	}

	bool LoadVectorAtlas(const std::string& path, VectorAtlas& atlas)
	{
		atlas = {};
		std::ifstream file(path, std::ios::binary);
		uint32_t magic = 0;
		uint32_t version = 0;
		if (!file || !Read(file, magic) || !Read(file, version) || magic != FileMagic || version != FileVersion)
			return false;
		uint32_t fontCount = 0;
		if (!Read(file, atlas.LayoutHash) || !Read(file, atlas.SourceHash) || !Read(file, atlas.Width) || !Read(file, atlas.Height) || !Read(file, atlas.DistanceRange)
			|| !Read(file, fontCount) || fontCount > 64 || atlas.Width > 16384 || atlas.Height > 16384)
			return false;
		atlas.Fonts.resize(fontCount);
		for (FontFace& font : atlas.Fonts)
		{
			uint32_t glyphs = 0;
			if (!Read(file, font.Ascender) || !Read(file, font.Descender) || !Read(file, font.LineGap) || !Read(file, glyphs) || glyphs > 65536)
				return false;
			for (uint32_t i = 0; i < glyphs; i++)
			{
				uint32_t codepoint = 0;
				AtlasGlyph glyph;
				if (!Read(file, codepoint) || !Read(file, glyph))
					return false;
				font.Glyphs[codepoint] = glyph;
			}
			uint32_t pairs = 0;
			if (!Read(file, pairs) || pairs > (1u << 22))
				return false;
			for (uint32_t i = 0; i < pairs; i++)
			{
				uint64_t pair = 0;
				float amount = 0.0f;
				if (!Read(file, pair) || !Read(file, amount))
					return false;
				font.Kerning[pair] = amount;
			}
		}
		uint32_t iconCount = 0;
		if (!Read(file, iconCount) || iconCount > 65536)
			return false;
		atlas.Icons.resize(iconCount);
		for (AtlasIcon& icon : atlas.Icons)
			if (!Read(file, icon))
				return false;
		atlas.Pixels.resize(size_t(atlas.Width) * atlas.Height * 4);
		return bool(file.read(reinterpret_cast<char*>(atlas.Pixels.data()), std::streamsize(atlas.Pixels.size())));
	}
}

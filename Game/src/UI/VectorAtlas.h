#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Game
{
	struct AtlasGlyph
	{
		float Advance = 0.0f;
		glm::vec2 PlaneMin{ 0.0f };
		glm::vec2 PlaneMax{ 0.0f };
		glm::vec2 UvMin{ 0.0f };
		glm::vec2 UvMax{ 0.0f };
		bool Visible = false;
	};

	inline uint64_t KerningPair(uint32_t left, uint32_t right)
	{
		return (uint64_t(left) << 32) | right;
	}

	struct FontFace
	{
		float Ascender = 0.0f;
		float Descender = 0.0f;
		float LineGap = 0.0f;
		std::unordered_map<uint32_t, AtlasGlyph> Glyphs;
		std::unordered_map<uint64_t, float> Kerning;

		const AtlasGlyph* Find(uint32_t codepoint) const;
		float GetKerning(uint32_t left, uint32_t right) const;
		float GetLineHeight() const { return Ascender - Descender + LineGap; }
	};

	struct AtlasIcon
	{
		glm::vec2 PlaneMin{ 0.0f };
		glm::vec2 PlaneMax{ 0.0f };
		glm::vec2 UvMin{ 0.0f };
		glm::vec2 UvMax{ 0.0f };
		bool Valid = false;
	};

	struct FontSource
	{
		std::string Path;
		std::vector<std::pair<uint32_t, uint32_t>> Ranges;
	};

	struct VectorAtlasDesc
	{
		std::vector<FontSource> Fonts;
		std::vector<std::string> Icons;
		float GlyphSize = 48.0f;
		float IconSize = 80.0f;
		float DistanceRange = 8.0f;
		uint32_t Width = 2048;
	};

	struct VectorAtlas
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
		float DistanceRange = 0.0f;
		uint64_t LayoutHash = 0;
		uint64_t SourceHash = 0;
		std::vector<uint8_t> Pixels;
		std::vector<FontFace> Fonts;
		std::vector<AtlasIcon> Icons;
		uint32_t RasterIcons = 0;
	};

	uint64_t HashAtlasLayout(const VectorAtlasDesc& desc);
	uint64_t HashAtlasSources(const VectorAtlasDesc& desc);
	bool SaveVectorAtlas(const std::string& path, const VectorAtlas& atlas);
	bool LoadVectorAtlas(const std::string& path, VectorAtlas& atlas);
}

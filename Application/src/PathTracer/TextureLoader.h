#pragma once
#include <RadianceEngine/Graphics/Common.h>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

struct TextureData
{
	uint32_t Width = 0;
	uint32_t Height = 0;
	uint32_t MipLevels = 1;
	Rdn::Format Format = Rdn::Format::Undefined;
	std::vector<uint8_t> Pixels;
};

struct HdrImage
{
	uint32_t Width = 0;
	uint32_t Height = 0;
	std::vector<float> Pixels;
};

bool ReadBinaryFile(const std::filesystem::path& path, std::vector<uint8_t>& bytes, std::string& error);
std::optional<TextureData> DecodeTexture(std::span<const uint8_t> file, bool srgb, std::string& error);
std::optional<HdrImage> DecodeHdrImage(std::span<const uint8_t> file, std::string& error);
bool IsBlockCompressed(Rdn::Format format);
uint32_t FullMipCount(uint32_t width, uint32_t height);

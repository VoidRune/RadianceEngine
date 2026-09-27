#include "TextureLoader.h"
#include "stb/stb_image.h"
#include <algorithm>
#include <bit>
#include <climits>
#include <cstring>
#include <format>
#include <fstream>

namespace
{
	constexpr uint32_t MakeFourCC(char a, char b, char c, char d)
	{
		return uint32_t(uint8_t(a)) | uint32_t(uint8_t(b)) << 8 | uint32_t(uint8_t(c)) << 16 | uint32_t(uint8_t(d)) << 24;
	}

	struct DdsPixelFormat
	{
		uint32_t Size;
		uint32_t Flags;
		uint32_t FourCC;
		uint32_t RGBBitCount;
		uint32_t RMask;
		uint32_t GMask;
		uint32_t BMask;
		uint32_t AMask;
	};

	struct DdsHeader
	{
		uint32_t Size;
		uint32_t Flags;
		uint32_t Height;
		uint32_t Width;
		uint32_t PitchOrLinearSize;
		uint32_t Depth;
		uint32_t MipMapCount;
		uint32_t Reserved1[11];
		DdsPixelFormat PixelFormat;
		uint32_t Caps;
		uint32_t Caps2;
		uint32_t Caps3;
		uint32_t Caps4;
		uint32_t Reserved2;
	};

	struct DdsHeaderDx10
	{
		uint32_t DxgiFormat;
		uint32_t ResourceDimension;
		uint32_t MiscFlag;
		uint32_t ArraySize;
		uint32_t MiscFlags2;
	};

	static_assert(sizeof(DdsHeader) == 124 && sizeof(DdsHeaderDx10) == 20);

	constexpr uint32_t DdsMagic = MakeFourCC('D', 'D', 'S', ' ');
	constexpr uint32_t DdpfAlphaPixels = 0x1;
	constexpr uint32_t DdpfFourCC = 0x4;
	constexpr uint32_t DdpfRgb = 0x40;
	constexpr uint32_t DdpfLuminance = 0x20000;
	constexpr uint32_t DdsCaps2Cubemap = 0x200;
	constexpr uint32_t DdsCaps2Volume = 0x200000;
	constexpr uint32_t DdsDimensionTexture2D = 3;
	constexpr uint32_t DdsMiscTextureCube = 0x4;

	enum class PixelConversion { None, OpaqueAlpha, Bgr24, Luminance8 };

	struct DdsFormat
	{
		Rdn::Format Format = Rdn::Format::Undefined;
		PixelConversion Conversion = PixelConversion::None;
	};

	struct BlockInfo
	{
		uint32_t Size = 1;
		uint32_t Bytes = 0;
	};

	Rdn::Format ColorFormat(Rdn::Format unorm, Rdn::Format srgbFormat, bool srgb)
	{
		return srgb ? srgbFormat : unorm;
	}

	DdsFormat FromDxgi(uint32_t dxgi, bool srgb)
	{
		using F = Rdn::Format;
		switch (dxgi)
		{
		case 2: return { F::R32G32B32A32_Sfloat };
		case 10: return { F::R16G16B16A16_Sfloat };
		case 11: return { F::R16G16B16A16_Unorm };
		case 16: return { F::R32G32_Sfloat };
		case 27: case 28: case 29: return { ColorFormat(F::R8G8B8A8_Unorm, F::R8G8B8A8_Srgb, srgb) };
		case 34: return { F::R16G16_Sfloat };
		case 35: return { F::R16G16_Unorm };
		case 41: return { F::R32_Sfloat };
		case 48: case 49: return { F::R8G8_Unorm };
		case 51: return { F::R8G8_Snorm };
		case 54: return { F::R16_Sfloat };
		case 56: return { F::R16_Unorm };
		case 60: case 61: return { F::R8_Unorm };
		case 70: case 71: case 72: return { ColorFormat(F::BC1_RGBA_Unorm, F::BC1_RGBA_Srgb, srgb) };
		case 73: case 74: case 75: return { ColorFormat(F::BC2_Unorm, F::BC2_Srgb, srgb) };
		case 76: case 77: case 78: return { ColorFormat(F::BC3_Unorm, F::BC3_Srgb, srgb) };
		case 79: case 80: return { F::BC4_Unorm };
		case 81: return { F::BC4_Snorm };
		case 82: case 83: return { F::BC5_Unorm };
		case 84: return { F::BC5_Snorm };
		case 87: case 90: case 91: return { ColorFormat(F::B8G8R8A8_Unorm, F::B8G8R8A8_Srgb, srgb) };
		case 88: case 92: case 93: return { ColorFormat(F::B8G8R8A8_Unorm, F::B8G8R8A8_Srgb, srgb), PixelConversion::OpaqueAlpha };
		case 94: case 95: return { F::BC6H_Ufloat };
		case 96: return { F::BC6H_Sfloat };
		case 97: case 98: case 99: return { ColorFormat(F::BC7_Unorm, F::BC7_Srgb, srgb) };
		default: return {};
		}
	}

	DdsFormat FromLegacy(const DdsPixelFormat& pixelFormat, bool srgb)
	{
		using F = Rdn::Format;
		if (pixelFormat.Flags & DdpfFourCC)
		{
			switch (pixelFormat.FourCC)
			{
			case MakeFourCC('D', 'X', 'T', '1'): return { ColorFormat(F::BC1_RGBA_Unorm, F::BC1_RGBA_Srgb, srgb) };
			case MakeFourCC('D', 'X', 'T', '2'): case MakeFourCC('D', 'X', 'T', '3'): return { ColorFormat(F::BC2_Unorm, F::BC2_Srgb, srgb) };
			case MakeFourCC('D', 'X', 'T', '4'): case MakeFourCC('D', 'X', 'T', '5'): return { ColorFormat(F::BC3_Unorm, F::BC3_Srgb, srgb) };
			case MakeFourCC('A', 'T', 'I', '1'): case MakeFourCC('B', 'C', '4', 'U'): return { F::BC4_Unorm };
			case MakeFourCC('B', 'C', '4', 'S'): return { F::BC4_Snorm };
			case MakeFourCC('A', 'T', 'I', '2'): case MakeFourCC('B', 'C', '5', 'U'): return { F::BC5_Unorm };
			case MakeFourCC('B', 'C', '5', 'S'): return { F::BC5_Snorm };
			case 36: return { F::R16G16B16A16_Unorm };
			case 111: return { F::R16_Sfloat };
			case 112: return { F::R16G16_Sfloat };
			case 113: return { F::R16G16B16A16_Sfloat };
			case 114: return { F::R32_Sfloat };
			case 115: return { F::R32G32_Sfloat };
			case 116: return { F::R32G32B32A32_Sfloat };
			default: return {};
			}
		}

		const bool rgb = (pixelFormat.Flags & DdpfRgb) != 0;
		if (rgb && pixelFormat.RGBBitCount == 32)
		{
			const PixelConversion alpha = (pixelFormat.Flags & DdpfAlphaPixels) && pixelFormat.AMask == 0xFF000000u ? PixelConversion::None : PixelConversion::OpaqueAlpha;
			if (pixelFormat.RMask == 0x000000FFu && pixelFormat.GMask == 0x0000FF00u && pixelFormat.BMask == 0x00FF0000u)
				return { ColorFormat(F::R8G8B8A8_Unorm, F::R8G8B8A8_Srgb, srgb), alpha };
			if (pixelFormat.RMask == 0x00FF0000u && pixelFormat.GMask == 0x0000FF00u && pixelFormat.BMask == 0x000000FFu)
				return { ColorFormat(F::B8G8R8A8_Unorm, F::B8G8R8A8_Srgb, srgb), alpha };
		}
		if (rgb && pixelFormat.RGBBitCount == 24 && pixelFormat.RMask == 0x00FF0000u && pixelFormat.GMask == 0x0000FF00u && pixelFormat.BMask == 0x000000FFu)
			return { ColorFormat(F::B8G8R8A8_Unorm, F::B8G8R8A8_Srgb, srgb), PixelConversion::Bgr24 };
		if ((pixelFormat.Flags & DdpfLuminance) && pixelFormat.RGBBitCount == 8)
			return { ColorFormat(F::R8G8B8A8_Unorm, F::R8G8B8A8_Srgb, srgb), PixelConversion::Luminance8 };
		return {};
	}

	BlockInfo GetBlockInfo(Rdn::Format format)
	{
		using F = Rdn::Format;
		switch (format)
		{
		case F::BC1_RGBA_Unorm: case F::BC1_RGBA_Srgb: case F::BC4_Unorm: case F::BC4_Snorm:
			return { 4, 8 };
		case F::BC2_Unorm: case F::BC2_Srgb: case F::BC3_Unorm: case F::BC3_Srgb: case F::BC5_Unorm: case F::BC5_Snorm:
		case F::BC6H_Ufloat: case F::BC6H_Sfloat: case F::BC7_Unorm: case F::BC7_Srgb:
			return { 4, 16 };
		case F::R8_Unorm:
			return { 1, 1 };
		case F::R8G8_Unorm: case F::R8G8_Snorm: case F::R16_Sfloat: case F::R16_Unorm:
			return { 1, 2 };
		case F::R8G8B8A8_Unorm: case F::R8G8B8A8_Srgb: case F::B8G8R8A8_Unorm: case F::B8G8R8A8_Srgb:
		case F::R16G16_Sfloat: case F::R16G16_Unorm: case F::R32_Sfloat:
			return { 1, 4 };
		case F::R16G16B16A16_Sfloat: case F::R16G16B16A16_Unorm: case F::R32G32_Sfloat:
			return { 1, 8 };
		case F::R32G32B32A32_Sfloat:
			return { 1, 16 };
		default:
			return {};
		}
	}

	uint64_t MipSize(BlockInfo block, uint32_t width, uint32_t height, uint32_t level)
	{
		const uint64_t w = std::max(1u, width >> level);
		const uint64_t h = std::max(1u, height >> level);
		return ((w + block.Size - 1) / block.Size) * ((h + block.Size - 1) / block.Size) * block.Bytes;
	}

	std::optional<TextureData> DecodeDds(std::span<const uint8_t> file, bool srgb, std::string& error)
	{
		if (file.size() < 4 + sizeof(DdsHeader))
		{
			error = "truncated DDS header";
			return std::nullopt;
		}
		DdsHeader header;
		std::memcpy(&header, file.data() + 4, sizeof(header));
		size_t offset = 4 + sizeof(DdsHeader);
		if (header.Size != sizeof(DdsHeader) || header.Width == 0 || header.Height == 0)
		{
			error = "invalid DDS header";
			return std::nullopt;
		}
		if (header.Caps2 & (DdsCaps2Cubemap | DdsCaps2Volume))
		{
			error = "cube maps and volume textures are not supported";
			return std::nullopt;
		}

		DdsFormat format;
		std::string formatName;
		if ((header.PixelFormat.Flags & DdpfFourCC) && header.PixelFormat.FourCC == MakeFourCC('D', 'X', '1', '0'))
		{
			if (file.size() < offset + sizeof(DdsHeaderDx10))
			{
				error = "truncated DX10 header";
				return std::nullopt;
			}
			DdsHeaderDx10 dx10;
			std::memcpy(&dx10, file.data() + offset, sizeof(dx10));
			offset += sizeof(dx10);
			if (dx10.ResourceDimension != DdsDimensionTexture2D || dx10.ArraySize > 1 || (dx10.MiscFlag & DdsMiscTextureCube))
			{
				error = "only single 2D textures are supported";
				return std::nullopt;
			}
			format = FromDxgi(dx10.DxgiFormat, srgb);
			formatName = std::format("DXGI format {}", dx10.DxgiFormat);
		}
		else
		{
			format = FromLegacy(header.PixelFormat, srgb);
			formatName = std::format("pixel format (flags {:#x}, FourCC {:#x}, {} bits)", header.PixelFormat.Flags, header.PixelFormat.FourCC, header.PixelFormat.RGBBitCount);
		}
		if (format.Format == Rdn::Format::Undefined)
		{
			error = "unsupported " + formatName;
			return std::nullopt;
		}

		BlockInfo source = GetBlockInfo(format.Format);
		if (format.Conversion == PixelConversion::Bgr24)
			source = { 1, 3 };
		else if (format.Conversion == PixelConversion::Luminance8)
			source = { 1, 1 };

		const uint64_t available = file.size() - offset;
		const uint32_t listedLevels = std::clamp(header.MipMapCount, 1u, FullMipCount(header.Width, header.Height));
		uint64_t dataSize = 0;
		uint32_t levels = 0;
		for (; levels < listedLevels; levels++)
		{
			const uint64_t size = MipSize(source, header.Width, header.Height, levels);
			if (dataSize + size > available)
				break;
			dataSize += size;
		}
		if (levels == 0)
		{
			error = "truncated DDS data";
			return std::nullopt;
		}

		TextureData texture;
		texture.Width = header.Width;
		texture.Height = header.Height;
		texture.MipLevels = levels;
		texture.Format = format.Format;
		const uint8_t* data = file.data() + offset;
		switch (format.Conversion)
		{
		case PixelConversion::None:
			texture.Pixels.assign(data, data + dataSize);
			break;
		case PixelConversion::OpaqueAlpha:
			texture.Pixels.assign(data, data + dataSize);
			for (size_t i = 3; i < texture.Pixels.size(); i += 4)
				texture.Pixels[i] = 0xFF;
			break;
		case PixelConversion::Bgr24:
			texture.Pixels.resize(dataSize / 3 * 4);
			for (size_t i = 0; i < dataSize / 3; i++)
			{
				std::memcpy(&texture.Pixels[i * 4], data + i * 3, 3);
				texture.Pixels[i * 4 + 3] = 0xFF;
			}
			break;
		case PixelConversion::Luminance8:
			texture.Pixels.resize(dataSize * 4);
			for (size_t i = 0; i < dataSize; i++)
			{
				std::memset(&texture.Pixels[i * 4], data[i], 3);
				texture.Pixels[i * 4 + 3] = 0xFF;
			}
			break;
		}
		return texture;
	}
}

bool ReadBinaryFile(const std::filesystem::path& path, std::vector<uint8_t>& bytes, std::string& error)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file)
	{
		error = "cannot open the file";
		return false;
	}
	const std::streamoff size = file.tellg();
	if (size < 0)
	{
		error = "cannot read the file size";
		return false;
	}
	bytes.resize(size_t(size));
	file.seekg(0);
	if (size > 0 && !file.read(reinterpret_cast<char*>(bytes.data()), size))
	{
		error = "reading the file failed";
		return false;
	}
	return true;
}

std::optional<TextureData> DecodeTexture(std::span<const uint8_t> file, bool srgb, std::string& error)
{
	uint32_t magic = 0;
	if (file.size() >= sizeof(magic))
		std::memcpy(&magic, file.data(), sizeof(magic));
	if (magic == DdsMagic)
		return DecodeDds(file, srgb, error);
	if (file.empty() || file.size() > size_t(INT_MAX))
	{
		error = std::format("unsupported image size of {} bytes", file.size());
		return std::nullopt;
	}

	int width = 0, height = 0, channels = 0;
	stbi_uc* pixels = stbi_load_from_memory(file.data(), int(file.size()), &width, &height, &channels, 4);
	if (!pixels)
	{
		error = stbi_failure_reason();
		return std::nullopt;
	}

	TextureData texture;
	texture.Width = uint32_t(width);
	texture.Height = uint32_t(height);
	texture.MipLevels = FullMipCount(texture.Width, texture.Height);
	texture.Format = srgb ? Rdn::Format::R8G8B8A8_Srgb : Rdn::Format::R8G8B8A8_Unorm;
	texture.Pixels.assign(pixels, pixels + size_t(width) * size_t(height) * 4);
	stbi_image_free(pixels);
	return texture;
}

std::optional<HdrImage> DecodeHdrImage(std::span<const uint8_t> file, std::string& error)
{
	if (file.empty() || file.size() > size_t(INT_MAX))
	{
		error = std::format("unsupported image size of {} bytes", file.size());
		return std::nullopt;
	}

	int width = 0, height = 0, channels = 0;
	float* pixels = stbi_loadf_from_memory(file.data(), int(file.size()), &width, &height, &channels, 4);
	if (!pixels)
	{
		error = stbi_failure_reason();
		return std::nullopt;
	}

	HdrImage image;
	image.Width = uint32_t(width);
	image.Height = uint32_t(height);
	image.Pixels.assign(pixels, pixels + size_t(width) * size_t(height) * 4);
	stbi_image_free(pixels);
	return image;
}

bool IsBlockCompressed(Rdn::Format format)
{
	return format >= Rdn::Format::BC1_RGB_Unorm && format <= Rdn::Format::BC7_Srgb;
}

uint32_t FullMipCount(uint32_t width, uint32_t height)
{
	return uint32_t(std::bit_width(std::max({ width, height, 1u })));
}

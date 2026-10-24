#include "TextureBaker.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <thread>

#include "tinygltf/stb_image.h"
#include "bc7enc/bc7enc.h"
#define RGBCX_IMPLEMENTATION
#include "bc7enc/rgbcx.h"

namespace Game
{
	namespace
	{
		struct FloatImage
		{
			uint32_t Width = 0;
			uint32_t Height = 0;
			std::vector<glm::vec4> Pixels;

			glm::vec4& At(uint32_t x, uint32_t y) { return Pixels[size_t(y) * Width + x]; }
			const glm::vec4& At(uint32_t x, uint32_t y) const { return Pixels[size_t(y) * Width + x]; }
		};

		float SrgbToLinear(float c)
		{
			return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
		}

		float LinearToSrgb(float c)
		{
			c = std::clamp(c, 0.0f, 1.0f);
			return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
		}

		uint8_t ToByte(float value)
		{
			return uint8_t(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
		}

		FloatImage Resample(const RgbaImage& source, uint32_t width, uint32_t height)
		{
			FloatImage result{ width, height, std::vector<glm::vec4>(size_t(width) * height) };
			const float sx = float(source.Width) / float(width);
			const float sy = float(source.Height) / float(height);
			const auto fetch = [&](uint32_t x, uint32_t y)
			{
				const uint8_t* p = &source.Pixels[(size_t(std::min(y, source.Height - 1)) * source.Width + std::min(x, source.Width - 1)) * 4];
				return glm::vec4(p[0], p[1], p[2], p[3]) / 255.0f;
			};
			for (uint32_t y = 0; y < height; y++)
				for (uint32_t x = 0; x < width; x++)
				{
					if (sx <= 1.0f && sy <= 1.0f)
					{
						const float fx = std::max((float(x) + 0.5f) * sx - 0.5f, 0.0f);
						const float fy = std::max((float(y) + 0.5f) * sy - 0.5f, 0.0f);
						const uint32_t x0 = uint32_t(fx);
						const uint32_t y0 = uint32_t(fy);
						const float tx = fx - float(x0);
						const float ty = fy - float(y0);
						result.At(x, y) = glm::mix(glm::mix(fetch(x0, y0), fetch(x0 + 1, y0), tx), glm::mix(fetch(x0, y0 + 1), fetch(x0 + 1, y0 + 1), tx), ty);
						continue;
					}
					const uint32_t x0 = uint32_t(float(x) * sx);
					const uint32_t y0 = uint32_t(float(y) * sy);
					const uint32_t x1 = std::max(uint32_t(std::ceil(float(x + 1) * sx)), x0 + 1);
					const uint32_t y1 = std::max(uint32_t(std::ceil(float(y + 1) * sy)), y0 + 1);
					glm::vec4 sum(0.0f);
					for (uint32_t j = y0; j < y1; j++)
						for (uint32_t i = x0; i < x1; i++)
							sum += fetch(i, j);
					result.At(x, y) = sum / float((x1 - x0) * (y1 - y0));
				}
			return result;
		}

		FloatImage Halve(const FloatImage& source)
		{
			FloatImage result{ std::max(source.Width / 2, 1u), std::max(source.Height / 2, 1u), {} };
			result.Pixels.resize(size_t(result.Width) * result.Height);
			for (uint32_t y = 0; y < result.Height; y++)
				for (uint32_t x = 0; x < result.Width; x++)
				{
					const uint32_t sx = std::min(2 * x, source.Width - 1);
					const uint32_t sy = std::min(2 * y, source.Height - 1);
					const uint32_t ex = std::min(sx + 1, source.Width - 1);
					const uint32_t ey = std::min(sy + 1, source.Height - 1);
					result.At(x, y) = 0.25f * (source.At(sx, sy) + source.At(ex, sy) + source.At(sx, ey) + source.At(ex, ey));
				}
			return result;
		}

		float Coverage(const FloatImage& image, float cutoff, float scale)
		{
			size_t covered = 0;
			for (const glm::vec4& pixel : image.Pixels)
				covered += pixel.a * scale >= cutoff ? 1 : 0;
			return float(covered) / float(std::max<size_t>(image.Pixels.size(), 1));
		}

		void PreserveCoverage(FloatImage& image, float cutoff, float target)
		{
			float low = 0.0f;
			float high = 8.0f;
			for (int i = 0; i < 16; i++)
			{
				const float middle = 0.5f * (low + high);
				(Coverage(image, cutoff, middle) < target ? low : high) = middle;
			}
			for (glm::vec4& pixel : image.Pixels)
				pixel.a = std::min(pixel.a * high, 1.0f);
		}

		template<typename Function>
		void ParallelFor(uint32_t count, const Function& function)
		{
			const uint32_t threads = std::clamp(std::thread::hardware_concurrency(), 1u, 32u);
			std::atomic<uint32_t> next{ 0 };
			std::vector<std::thread> workers;
			for (uint32_t t = 0; t < threads; t++)
				workers.emplace_back([&]()
				{
					for (uint32_t i = next++; i < count; i = next++)
						function(i);
				});
			for (std::thread& worker : workers)
				worker.join();
		}

		void EncodeLevel(const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height, TextureFormat format, std::vector<uint8_t>& output)
		{
			if (format == TextureFormat::Rgba8 || format == TextureFormat::Rgba8Srgb)
			{
				output.insert(output.end(), rgba.begin(), rgba.end());
				return;
			}
			const uint32_t blocksX = (width + 3) / 4;
			const uint32_t blocksY = (height + 3) / 4;
			const size_t base = output.size();
			output.resize(base + size_t(blocksX) * blocksY * 16);
			bc7enc_compress_block_params params;
			bc7enc_compress_block_params_init(&params);
			params.m_uber_level = 1;
			if (format == TextureFormat::Bc7)
				bc7enc_compress_block_params_init_linear_weights(&params);
			ParallelFor(blocksY, [&](uint32_t by)
			{
				uint8_t block[64];
				for (uint32_t bx = 0; bx < blocksX; bx++)
				{
					for (uint32_t y = 0; y < 4; y++)
						for (uint32_t x = 0; x < 4; x++)
						{
							const uint32_t sx = std::min(bx * 4 + x, width - 1);
							const uint32_t sy = std::min(by * 4 + y, height - 1);
							std::memcpy(&block[(y * 4 + x) * 4], &rgba[(size_t(sy) * width + sx) * 4], 4);
						}
					uint8_t* target = &output[base + (size_t(by) * blocksX + bx) * 16];
					if (format == TextureFormat::Bc5)
						rgbcx::encode_bc5(target, block, 0, 1, 4);
					else
						bc7enc_compress_block(target, block, &params);
				}
			});
		}

		ModelTexture Encode(const std::string& name, const std::vector<std::vector<uint8_t>>& levels, uint32_t width, uint32_t height, TextureFormat format, bool repeat)
		{
			ModelTexture texture;
			texture.Name = name;
			texture.Width = width;
			texture.Height = height;
			texture.MipCount = uint32_t(levels.size());
			texture.Format = format;
			texture.Repeat = repeat;
			for (uint32_t mip = 0; mip < levels.size(); mip++)
				EncodeLevel(levels[mip], std::max(width >> mip, 1u), std::max(height >> mip, 1u), format, texture.Pixels);
			return texture;
		}

		std::vector<uint8_t> Bytes(const FloatImage& image, const auto& convert)
		{
			std::vector<uint8_t> bytes(image.Pixels.size() * 4);
			for (size_t i = 0; i < image.Pixels.size(); i++)
			{
				const glm::vec4 value = convert(image.Pixels[i], i);
				for (int c = 0; c < 4; c++)
					bytes[i * 4 + size_t(c)] = ToByte(value[c]);
			}
			return bytes;
		}

		void TargetSize(const PbrBakeInput& input, uint32_t& width, uint32_t& height)
		{
			const RgbaImage* source = input.Albedo ? input.Albedo : input.Normal ? input.Normal : input.Surface ? input.Surface : input.Emission;
			width = source ? source->Width : 1;
			height = source ? source->Height : 1;
			while ((width > input.MaxSize || height > input.MaxSize) && width > 1 && height > 1)
			{
				width = std::max(width / 2, 1u);
				height = std::max(height / 2, 1u);
			}
		}
	}

	bool LoadRgbaImage(const std::filesystem::path& path, RgbaImage& image)
	{
		int width = 0;
		int height = 0;
		int components = 0;
		stbi_uc* data = stbi_load(path.string().c_str(), &width, &height, &components, 4);
		if (!data)
			return false;
		image = MakeRgbaImage(data, width, height, 4);
		stbi_image_free(data);
		return true;
	}

	RgbaImage MakeRgbaImage(const uint8_t* data, int width, int height, int components)
	{
		RgbaImage image;
		image.Width = uint32_t(width);
		image.Height = uint32_t(height);
		image.Pixels.resize(size_t(width) * size_t(height) * 4);
		for (size_t i = 0; i < size_t(width) * size_t(height); i++)
			for (size_t c = 0; c < 4; c++)
				image.Pixels[i * 4 + c] = c < size_t(components) ? data[i * size_t(components) + c] : uint8_t(255);
		return image;
	}

	void InitTextureEncoders()
	{
		bc7enc_compress_block_init();
		rgbcx::init();
	}

	PbrBakeOutput BakePbrTextures(const PbrBakeInput& input)
	{
		uint32_t width = 0;
		uint32_t height = 0;
		TargetSize(input, width, height);
		uint32_t mipCount = 1;
		while ((std::max(width, height) >> mipCount) > 0)
			mipCount++;

		std::vector<FloatImage> normals;
		if (input.Normal)
		{
			FloatImage level = Resample(*input.Normal, width, height);
			for (glm::vec4& pixel : level.Pixels)
			{
				const glm::vec3 n = glm::vec3(pixel) * 2.0f - 1.0f;
				const float length = glm::length(n);
				pixel = glm::vec4(length > 1e-6f ? n / length : glm::vec3(0.0f, 0.0f, 1.0f), 0.0f);
			}
			normals.push_back(std::move(level));
			for (uint32_t mip = 1; mip < mipCount; mip++)
				normals.push_back(Halve(normals.back()));
		}

		PbrBakeOutput output;
		{
			FloatImage level;
			if (input.Albedo)
				level = Resample(*input.Albedo, width, height);
			else
				level = { width, height, std::vector<glm::vec4>(size_t(width) * height, glm::vec4(1.0f)) };
			for (glm::vec4& pixel : level.Pixels)
				pixel = glm::vec4(SrgbToLinear(pixel.r), SrgbToLinear(pixel.g), SrgbToLinear(pixel.b), pixel.a);
			if (input.Alpha)
			{
				const FloatImage alpha = Resample(*input.Alpha, width, height);
				for (size_t i = 0; i < level.Pixels.size(); i++)
					level.Pixels[i].a = alpha.Pixels[i].r;
			}
			else if (input.HeightInAlpha)
				for (glm::vec4& pixel : level.Pixels)
					pixel.a = 1.0f;
			const float coverage = input.AlphaCutoff > 0.0f ? Coverage(level, input.AlphaCutoff, 1.0f) : 0.0f;
			std::vector<std::vector<uint8_t>> levels;
			for (uint32_t mip = 0; mip < mipCount; mip++)
			{
				if (mip > 0)
				{
					level = Halve(level);
					if (input.AlphaCutoff > 0.0f)
						PreserveCoverage(level, input.AlphaCutoff, coverage);
				}
				levels.push_back(Bytes(level, [](const glm::vec4& p, size_t) { return glm::vec4(LinearToSrgb(p.r), LinearToSrgb(p.g), LinearToSrgb(p.b), p.a); }));
			}
			output.Albedo = Encode(input.Name + "_albedo", levels, width, height, TextureFormat::Bc7Srgb, input.Repeat);
		}

		if (input.Normal)
		{
			std::vector<std::vector<uint8_t>> levels;
			for (const FloatImage& level : normals)
				levels.push_back(Bytes(level, [](const glm::vec4& p, size_t)
				{
					const glm::vec3 n = glm::length(glm::vec3(p)) > 1e-6f ? glm::normalize(glm::vec3(p)) : glm::vec3(0.0f, 0.0f, 1.0f);
					return glm::vec4(n.x * 0.5f + 0.5f, n.y * 0.5f + 0.5f, 0.0f, 1.0f);
				}));
			output.Normal = Encode(input.Name + "_normal", levels, width, height, TextureFormat::Bc5, input.Repeat);
		}

		if (input.Surface || input.Emission)
		{
			FloatImage level = input.Surface ? Resample(*input.Surface, width, height) : FloatImage{ width, height, std::vector<glm::vec4>(size_t(width) * height, glm::vec4(1.0f)) };
			for (glm::vec4& pixel : level.Pixels)
			{
				const float alpha = pixel.g * pixel.g;
				pixel.g = alpha * alpha;
				pixel.a = 1.0f;
			}
			if (input.Emission)
			{
				const FloatImage glow = Resample(*input.Emission, width, height);
				for (size_t i = 0; i < level.Pixels.size(); i++)
					level.Pixels[i].a = std::max({ glow.Pixels[i].r, glow.Pixels[i].g, glow.Pixels[i].b });
			}
			std::vector<std::vector<uint8_t>> levels;
			for (uint32_t mip = 0; mip < mipCount; mip++)
			{
				if (mip > 0)
					level = Halve(level);
				levels.push_back(Bytes(level, [&](const glm::vec4& p, size_t i)
				{
					float alpha2 = p.g;
					if (!normals.empty())
					{
						const float length = std::clamp(glm::length(glm::vec3(normals[mip].Pixels[i])), 1e-4f, 1.0f);
						alpha2 += 2.0f * (1.0f - length) / length;
					}
					return glm::vec4(p.r, std::sqrt(std::sqrt(std::min(alpha2, 1.0f))), p.b, p.a);
				}));
			}
			output.Surface = Encode(input.Name + "_surface", levels, width, height, TextureFormat::Bc7, input.Repeat);
		}
		return output;
	}
}

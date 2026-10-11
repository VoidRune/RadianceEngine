#include "EnvironmentBaker.h"
#include <glm/gtc/packing.hpp>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <format>
#include <thread>
#include <vector>

#include "tinygltf/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

namespace Game
{
	namespace
	{
		constexpr float Pi = 3.14159265358979f;
		constexpr float SunRadius = 0.07f;
		constexpr float RingRadius = 0.14f;
		constexpr uint32_t PrefilterSamples = 128;

		struct Equirect
		{
			uint32_t Width = 0;
			uint32_t Height = 0;
			std::vector<glm::vec3> Pixels;

			glm::vec3& At(uint32_t x, uint32_t y) { return Pixels[size_t(y) * Width + x]; }
			const glm::vec3& At(uint32_t x, uint32_t y) const { return Pixels[size_t(y) * Width + x]; }
		};

		glm::vec3 Direction(float u, float v)
		{
			const float theta = v * Pi;
			const float phi = (u - 0.5f) * 2.0f * Pi;
			return { -std::sin(theta) * std::sin(phi), std::cos(theta), -std::sin(theta) * std::cos(phi) };
		}

		glm::vec2 Coordinates(const glm::vec3& d)
		{
			return { 0.5f + std::atan2(-d.x, -d.z) / (2.0f * Pi), std::acos(std::clamp(d.y, -1.0f, 1.0f)) / Pi };
		}

		glm::vec3 TexelDirection(const Equirect& image, uint32_t x, uint32_t y)
		{
			return Direction((float(x) + 0.5f) / float(image.Width), (float(y) + 0.5f) / float(image.Height));
		}

		float TexelSolidAngle(const Equirect& image, uint32_t y)
		{
			const float theta = (float(y) + 0.5f) / float(image.Height) * Pi;
			return (2.0f * Pi / float(image.Width)) * (Pi / float(image.Height)) * std::sin(theta);
		}

		float Luminance(const glm::vec3& c)
		{
			return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
		}

		Equirect Halve(const Equirect& source)
		{
			Equirect result{ std::max(source.Width / 2, 1u), std::max(source.Height / 2, 1u), {} };
			result.Pixels.resize(size_t(result.Width) * result.Height);
			for (uint32_t y = 0; y < result.Height; y++)
				for (uint32_t x = 0; x < result.Width; x++)
				{
					const uint32_t sx = std::min(2 * x, source.Width - 1);
					const uint32_t sy = std::min(2 * y, source.Height - 1);
					const uint32_t ex = (sx + 1) % source.Width;
					const uint32_t ey = std::min(sy + 1, source.Height - 1);
					result.At(x, y) = 0.25f * (source.At(sx, sy) + source.At(ex, sy) + source.At(sx, ey) + source.At(ex, ey));
				}
			return result;
		}

		glm::vec3 Bilinear(const Equirect& image, glm::vec2 uv)
		{
			const float fx = uv.x * float(image.Width) - 0.5f;
			const float fy = std::clamp(uv.y * float(image.Height) - 0.5f, 0.0f, float(image.Height - 1));
			const int x0 = int(std::floor(fx));
			const int y0 = int(std::floor(fy));
			const float tx = fx - float(x0);
			const float ty = fy - float(y0);
			const auto at = [&](int x, int y)
			{
				const uint32_t wx = uint32_t(((x % int(image.Width)) + int(image.Width)) % int(image.Width));
				const uint32_t wy = uint32_t(std::clamp(y, 0, int(image.Height) - 1));
				return image.At(wx, wy);
			};
			return glm::mix(glm::mix(at(x0, y0), at(x0 + 1, y0), tx), glm::mix(at(x0, y0 + 1), at(x0 + 1, y0 + 1), tx), ty);
		}

		glm::vec3 SamplePyramid(const std::vector<Equirect>& pyramid, glm::vec2 uv, float lod)
		{
			lod = std::clamp(lod, 0.0f, float(pyramid.size() - 1));
			const uint32_t low = uint32_t(lod);
			const uint32_t high = std::min(low + 1, uint32_t(pyramid.size() - 1));
			return glm::mix(Bilinear(pyramid[low], uv), Bilinear(pyramid[high], uv), lod - float(low));
		}

		glm::vec2 Hammersley(uint32_t i, uint32_t count)
		{
			uint32_t bits = i;
			bits = (bits << 16u) | (bits >> 16u);
			bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
			bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
			bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
			bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
			return { float(i) / float(count), float(bits) * 2.3283064365386963e-10f };
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

		std::array<float, 9> ShBasis(const glm::vec3& d)
		{
			return {
				0.282095f,
				0.488603f * d.y, 0.488603f * d.z, 0.488603f * d.x,
				1.092548f * d.x * d.y, 1.092548f * d.y * d.z, 0.315392f * (3.0f * d.z * d.z - 1.0f), 1.092548f * d.x * d.z, 0.546274f * (d.x * d.x - d.y * d.y),
			};
		}

		Equirect Prefilter(const std::vector<Equirect>& pyramid, uint32_t width, uint32_t height, float roughness)
		{
			Equirect result{ width, height, std::vector<glm::vec3>(size_t(width) * height) };
			const float alpha = std::max(roughness * roughness, 1e-3f);
			const float sourceTexel = 4.0f * Pi / float(pyramid[0].Width * pyramid[0].Height);
			ParallelFor(height, [&](uint32_t y)
			{
				for (uint32_t x = 0; x < width; x++)
				{
					const glm::vec3 n = TexelDirection(result, x, y);
					const glm::vec3 up = std::abs(n.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
					const glm::vec3 tangent = glm::normalize(glm::cross(up, n));
					const glm::vec3 bitangent = glm::cross(n, tangent);
					glm::vec3 sum(0.0f);
					float weight = 0.0f;
					for (uint32_t i = 0; i < PrefilterSamples; i++)
					{
						const glm::vec2 xi = Hammersley(i, PrefilterSamples);
						const float phi = 2.0f * Pi * xi.x;
						const float cosTheta = std::sqrt((1.0f - xi.y) / (1.0f + (alpha * alpha - 1.0f) * xi.y));
						const float sinTheta = std::sqrt(std::max(1.0f - cosTheta * cosTheta, 0.0f));
						const glm::vec3 h = tangent * (sinTheta * std::cos(phi)) + bitangent * (sinTheta * std::sin(phi)) + n * cosTheta;
						const glm::vec3 l = 2.0f * glm::dot(n, h) * h - n;
						const float cosL = glm::dot(n, l);
						if (cosL <= 0.0f)
							continue;
						const float a2 = alpha * alpha;
						const float d = cosTheta * cosTheta * (a2 - 1.0f) + 1.0f;
						const float pdf = a2 / (Pi * d * d) * 0.25f;
						const float sampleAngle = 1.0f / (float(PrefilterSamples) * pdf);
						const float lod = 0.5f * std::log2(sampleAngle / sourceTexel) + 1.0f;
						sum += SamplePyramid(pyramid, Coordinates(l), lod) * cosL;
						weight += cosL;
					}
					result.At(x, y) = weight > 0.0f ? sum / weight : glm::vec3(0.0f);
				}
			});
			return result;
		}
	}

	bool WriteSkyImage(const std::filesystem::path& path, const SkyImage& sky)
	{
		return stbi_write_hdr(path.string().c_str(), int(sky.Width), int(sky.Height), 3, sky.Pixels.data()) != 0;
	}

	bool BakeEnvironment(const std::filesystem::path& path, const std::string& name, uint32_t width, ModelEnvironment& environment, std::string& report, std::string& error, SkyImage* sky)
	{
		int sourceWidth = 0;
		int sourceHeight = 0;
		int components = 0;
		float* data = stbi_loadf(path.string().c_str(), &sourceWidth, &sourceHeight, &components, 3);
		if (!data)
		{
			error = path.string() + " can't be read as an HDR image";
			return false;
		}
		Equirect source{ uint32_t(sourceWidth), uint32_t(sourceHeight), std::vector<glm::vec3>(size_t(sourceWidth) * size_t(sourceHeight)) };
		for (size_t i = 0; i < source.Pixels.size(); i++)
			source.Pixels[i] = glm::max(glm::vec3(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]), glm::vec3(0.0f));
		stbi_image_free(data);

		uint32_t peakX = 0;
		uint32_t peakY = 0;
		float peak = 0.0f;
		std::vector<float> luminances;
		luminances.reserve(source.Pixels.size());
		for (uint32_t y = 0; y < source.Height; y++)
			for (uint32_t x = 0; x < source.Width; x++)
			{
				const float luminance = Luminance(source.At(x, y));
				luminances.push_back(luminance);
				if (luminance > peak)
				{
					peak = luminance;
					peakX = x;
					peakY = y;
				}
			}
		std::nth_element(luminances.begin(), luminances.begin() + ptrdiff_t(luminances.size() / 2), luminances.end());
		const float median = luminances[luminances.size() / 2];

		environment = {};
		environment.Name = name;
		if (peak > 200.0f * median)
		{
			const glm::vec3 center = TexelDirection(source, peakX, peakY);
			const float sunCos = std::cos(SunRadius);
			const float ringCos = std::cos(RingRadius);
			glm::vec3 ring(0.0f);
			float ringWeight = 0.0f;
			for (uint32_t y = 0; y < source.Height; y++)
				for (uint32_t x = 0; x < source.Width; x++)
				{
					const float c = glm::dot(TexelDirection(source, x, y), center);
					if (c < sunCos && c >= ringCos)
					{
						const float area = TexelSolidAngle(source, y);
						ring += source.At(x, y) * area;
						ringWeight += area;
					}
				}
			ring = ringWeight > 0.0f ? ring / ringWeight : glm::vec3(0.0f);
			glm::vec3 irradiance(0.0f);
			glm::vec3 direction(0.0f);
			for (uint32_t y = 0; y < source.Height; y++)
				for (uint32_t x = 0; x < source.Width; x++)
				{
					const glm::vec3 d = TexelDirection(source, x, y);
					if (glm::dot(d, center) < sunCos)
						continue;
					glm::vec3& pixel = source.At(x, y);
					const glm::vec3 excess = glm::max(pixel - ring, glm::vec3(0.0f));
					const float area = TexelSolidAngle(source, y);
					irradiance += excess * area;
					direction += d * (Luminance(excess) * area);
					pixel = glm::min(pixel, ring);
				}
			if (glm::length(direction) > 0.0f)
			{
				environment.SunDirection = -glm::normalize(direction);
				environment.SunIrradiance = irradiance;
			}
		}

		if (sky)
		{
			sky->Width = source.Width;
			sky->Height = source.Height;
			sky->Pixels.resize(source.Pixels.size() * 3);
			for (size_t i = 0; i < source.Pixels.size(); i++)
				for (int c = 0; c < 3; c++)
					sky->Pixels[i * 3 + size_t(c)] = source.Pixels[i][c];
		}
		Equirect work = source;
		while (work.Width > 1024)
			work = Halve(work);
		std::array<glm::vec3, 9> radiance{};
		for (uint32_t y = 0; y < work.Height; y++)
		{
			const float area = TexelSolidAngle(work, y);
			for (uint32_t x = 0; x < work.Width; x++)
			{
				const std::array<float, 9> basis = ShBasis(TexelDirection(work, x, y));
				for (size_t k = 0; k < 9; k++)
					radiance[k] += work.At(x, y) * (basis[k] * area);
			}
		}
		const float bands[9] = { 1.0f, 2.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f };
		for (size_t k = 0; k < 9; k++)
			environment.Irradiance[k] = radiance[k] * bands[k];

		std::vector<Equirect> pyramid{ work };
		while (pyramid.back().Width > 4)
			pyramid.push_back(Halve(pyramid.back()));

		width = std::clamp(width, 16u, work.Width);
		environment.Width = width;
		environment.Height = width / 2;
		environment.MipCount = 1;
		while ((width >> environment.MipCount) >= 8)
			environment.MipCount++;
		for (uint32_t mip = 0; mip < environment.MipCount; mip++)
		{
			const uint32_t w = width >> mip;
			const uint32_t h = std::max(w / 2, 1u);
			Equirect level;
			if (mip == 0)
			{
				level = work;
				while (level.Width > w)
					level = Halve(level);
			}
			else
				level = Prefilter(pyramid, w, h, float(mip) / float(environment.MipCount - 1));
			for (const glm::vec3& pixel : level.Pixels)
			{
				environment.Pixels.push_back(glm::packHalf1x16(pixel.r));
				environment.Pixels.push_back(glm::packHalf1x16(pixel.g));
				environment.Pixels.push_back(glm::packHalf1x16(pixel.b));
				environment.Pixels.push_back(glm::packHalf1x16(1.0f));
			}
		}
		const auto evaluate = [&](const glm::vec3& n)
		{
			const std::array<float, 9> basis = ShBasis(n);
			glm::vec3 sum(0.0f);
			for (size_t k = 0; k < 9; k++)
				sum += environment.Irradiance[k] * basis[k];
			return Luminance(sum);
		};
		const glm::vec3 sun = -environment.SunDirection;
		report = std::format("{}: sun at {:.1f} deg elevation, {:.1f} deg azimuth, irradiance {:.2f} {:.2f} {:.2f}; sky E/pi up {:.3f}, down {:.3f}; peak/median {:.0f}",
			name, std::asin(std::clamp(sun.y, -1.0f, 1.0f)) * 180.0f / Pi, std::atan2(sun.x, sun.z) * 180.0f / Pi,
			environment.SunIrradiance.r, environment.SunIrradiance.g, environment.SunIrradiance.b,
			evaluate({ 0.0f, 1.0f, 0.0f }), evaluate({ 0.0f, -1.0f, 0.0f }), median > 0.0f ? peak / median : 0.0f);
		return true;
	}
}

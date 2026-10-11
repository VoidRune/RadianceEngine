#pragma once
#include "Rendering/ModelData.h"
#include <filesystem>
#include <string>
#include <vector>

namespace Game
{
	struct RgbaImage
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
		std::vector<uint8_t> Pixels;

		bool IsEmpty() const { return Pixels.empty(); }
	};

	bool LoadRgbaImage(const std::filesystem::path& path, RgbaImage& image);
	RgbaImage MakeRgbaImage(const uint8_t* data, int width, int height, int components);

	struct PbrBakeInput
	{
		std::string Name;
		const RgbaImage* Albedo = nullptr;
		const RgbaImage* Alpha = nullptr;
		const RgbaImage* Normal = nullptr;
		const RgbaImage* Surface = nullptr;
		uint32_t MaxSize = 1024;
		float AlphaCutoff = 0.0f;
		bool HeightInAlpha = false;
		bool Repeat = false;
	};

	struct PbrBakeOutput
	{
		ModelTexture Albedo;
		ModelTexture Normal;
		ModelTexture Surface;
	};

	void InitTextureEncoders();
	PbrBakeOutput BakePbrTextures(const PbrBakeInput& input);
}

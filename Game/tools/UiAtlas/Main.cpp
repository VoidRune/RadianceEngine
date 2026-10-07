#include "AtlasGenerator.h"
#include "UI/UiAtlasSources.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <string_view>

int main(int argc, char** argv)
{
	using namespace Game;
	bool force = false;
	for (int i = 1; i < argc; i++)
	{
		if (std::string_view(argv[i]) == "--force")
		{
			force = true;
			continue;
		}
		std::error_code error;
		std::filesystem::current_path(argv[i], error);
		if (error)
		{
			std::fprintf(stderr, "UiAtlas : error : can't open the game folder %s\n", argv[i]);
			return 1;
		}
	}

	const VectorAtlasDesc desc = MakeUiAtlasDesc();
	const uint64_t layoutHash = HashAtlasLayout(desc);
	const uint64_t sourceHash = HashAtlasSources(desc);
	if (VectorAtlas baked; !force && LoadVectorAtlas(UiAtlasPath, baked) && baked.LayoutHash == layoutHash && baked.SourceHash == sourceHash)
	{
		std::printf("UiAtlas: %s is up to date\n", UiAtlasPath);
		return 0;
	}

	const auto start = std::chrono::steady_clock::now();
	VectorAtlas atlas;
	std::string error;
	if (!GenerateVectorAtlas(desc, atlas, error))
	{
		std::fprintf(stderr, "UiAtlas : error : %s\n", error.c_str());
		return 1;
	}
	for (size_t i = 0; i < atlas.Icons.size(); i++)
		if (!atlas.Icons[i].Valid)
			std::fprintf(stderr, "UiAtlas : warning : %s couldn't be read\n", desc.Icons[i].c_str());
	atlas.LayoutHash = layoutHash;
	atlas.SourceHash = sourceHash;
	if (!SaveVectorAtlas(UiAtlasPath, atlas))
	{
		std::fprintf(stderr, "UiAtlas : error : can't write %s\n", UiAtlasPath);
		return 1;
	}
	const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	std::printf("UiAtlas: baked %s, %ux%u with %zu fonts and %zu icons (%u drawn from rasters) in %.2f s\n",
		UiAtlasPath, atlas.Width, atlas.Height, std::size(UiFontFiles), std::size(UiIconFiles), atlas.RasterIcons, seconds);
	return 0;
}

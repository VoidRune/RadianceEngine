#include "EnvironmentBaker.h"
#include "GltfImporter.h"
#include "TextureBaker.h"
#include "Rendering/ModelSources.h"
#include "Rendering/ModelSurfaces.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string_view>
#include <vector>

namespace
{
	constexpr uint32_t BakerVersion = 9;

	uint64_t Hash(uint64_t hash, const void* data, size_t size)
	{
		const uint8_t* bytes = static_cast<const uint8_t*>(data);
		for (size_t i = 0; i < size; i++)
			hash = (hash ^ bytes[i]) * 0x100000001B3ull;
		return hash;
	}

	bool UsesSwatches(const Game::ModelPack& pack)
	{
		using namespace Game;
		return std::any_of(pack.Sources.begin(), pack.Sources.end(), [](const ModelSource& source) { return source.Kind == ModelSourceKind::Character || source.Kind == ModelSourceKind::Prop || source.Kind == ModelSourceKind::PropFolder; });
	}

	uint64_t HashPack(const std::filesystem::path& root, const Game::ModelPack& pack)
	{
		using namespace Game;
		uint64_t hash = 0xCBF29CE484222325ull;
		const uint32_t settings[] = { BakerVersion, ModelTextureMaxSize, uint32_t(ModelClipFrameRate) };
		hash = Hash(hash, settings, sizeof(settings));
		std::set<std::string> folders;
		for (const ModelSource& source : pack.Sources)
		{
			hash = Hash(hash, source.Path, std::string_view(source.Path).size());
			hash = Hash(hash, &source.Kind, sizeof(source.Kind));
			hash = Hash(hash, &source.TextureSize, sizeof(source.TextureSize));
			hash = Hash(hash, &source.Size, sizeof(source.Size));
			hash = Hash(hash, &source.SplitNodes, sizeof(source.SplitNodes));
			const std::string_view path = source.Path;
			folders.insert(std::string(path.substr(0, path.find('/'))));
		}
		if (UsesSwatches(pack))
		{
			hash = Hash(hash, SwatchMaterials, sizeof(SwatchMaterials));
			for (const SwatchTable& table : SwatchTables)
			{
				hash = Hash(hash, table.Texture, std::string_view(table.Texture).size());
				for (const char* row : table.Rows)
					hash = Hash(hash, row, std::string_view(row).size());
			}
			for (const SwatchOverride& entry : SwatchOverrides)
			{
				hash = Hash(hash, entry.Mesh, std::string_view(entry.Mesh).size());
				hash = Hash(hash, &entry.Key, sizeof(entry.Key));
			}
		}
		std::vector<std::filesystem::path> files;
		for (const std::string& folder : folders)
		{
			std::error_code error;
			for (const auto& entry : std::filesystem::recursive_directory_iterator(root / folder, error))
				if (entry.is_regular_file())
					files.push_back(entry.path());
		}
		std::sort(files.begin(), files.end());
		std::vector<char> buffer(1 << 20);
		for (const std::filesystem::path& file : files)
		{
			const std::string relative = std::filesystem::relative(file, root).generic_string();
			hash = Hash(hash, relative.data(), relative.size());
			std::ifstream stream(file, std::ios::binary);
			while (stream)
			{
				stream.read(buffer.data(), std::streamsize(buffer.size()));
				hash = Hash(hash, buffer.data(), size_t(stream.gcount()));
			}
		}
		return hash;
	}

	void ReportSwatches(const Game::ModelLibraryData& library)
	{
		using namespace Game;
		std::vector<std::map<std::string, uint32_t>> usage(library.Textures.size() * SwatchCells);
		for (const ModelMesh& mesh : library.Meshes)
		{
			if (mesh.Texture == NoTexture)
				continue;
			for (uint32_t i = mesh.FirstIndex; i + 2 < mesh.FirstIndex + mesh.IndexCount; i += 3)
			{
				glm::vec2 uv(0.0f);
				for (uint32_t k = 0; k < 3; k++)
				{
					const ModelVertex& vertex = library.Vertices[library.Indices[i + k]];
					uv += glm::vec2(vertex.U, vertex.V) / 3.0f;
				}
				const uint32_t column = std::min(uint32_t((uv.x - std::floor(uv.x)) * SwatchGrid), SwatchGrid - 1);
				const uint32_t row = std::min(uint32_t((uv.y - std::floor(uv.y)) * SwatchGrid), SwatchGrid - 1);
				usage[size_t(mesh.Texture) * SwatchCells + row * SwatchGrid + column][mesh.Name]++;
			}
		}
		for (size_t t = 0; t < library.Textures.size(); t++)
		{
			const ModelTexture& texture = library.Textures[t];
			if (texture.Format != TextureFormat::Rgba8Srgb)
				continue;
			std::printf("%s\n", texture.Name.c_str());
			for (uint32_t cell = 0; cell < SwatchCells; cell++)
			{
				const std::map<std::string, uint32_t>& meshes = usage[t * SwatchCells + cell];
				if (meshes.empty())
					continue;
				const uint32_t x = (cell % SwatchGrid) * texture.Width / SwatchGrid + texture.Width / (2 * SwatchGrid);
				const uint32_t y = (cell / SwatchGrid) * texture.Height / SwatchGrid + texture.Height / (2 * SwatchGrid);
				const uint8_t* pixel = &texture.Pixels[(size_t(y) * texture.Width + x) * 4];
				std::printf("  row %u col %u #%02X%02X%02X:", cell / SwatchGrid, cell % SwatchGrid, pixel[0], pixel[1], pixel[2]);
				for (const auto& [name, triangles] : meshes)
					std::printf(" %s(%u)", name.c_str(), triangles);
				std::printf("\n");
			}
		}
	}

	void ListPack(const Game::ModelPack& pack)
	{
		using namespace Game;
		ModelLibraryData library;
		if (!LoadModelLibrary(pack.Path, library))
		{
			std::fprintf(stderr, "ModelBaker : error : can't read %s\n", pack.Path);
			return;
		}
		constexpr const char* formats[] = { "rgba8", "rgba8 srgb", "bc7", "bc7 srgb", "bc5" };
		std::printf("%s\n", pack.Path);
		for (const ModelTexture& texture : library.Textures)
			std::printf("  texture %-48s %4ux%-4u %-10s %2u mips %7.2f MB\n", texture.Name.c_str(), texture.Width, texture.Height, formats[size_t(texture.Format)], texture.MipCount, double(texture.Pixels.size()) / 1048576.0);
		for (const ModelMesh& mesh : library.Meshes)
			std::printf("  mesh %-48s %7u triangles, %.2f x %.2f x %.2f m, cutoff %.2f\n", mesh.Name.c_str(), mesh.IndexCount / 3, mesh.Max.x - mesh.Min.x, mesh.Max.y - mesh.Min.y, mesh.Max.z - mesh.Min.z, mesh.AlphaCutoff);
		for (const ModelMaterial& material : library.Materials)
			std::printf("  material %s, %.2f m\n", material.Name.c_str(), material.Size);
	}

	int BakePack(const std::filesystem::path& assets, const Game::ModelPack& pack, bool force, bool swatches)
	{
		using namespace Game;
		const uint64_t sourceHash = HashPack(assets, pack);
		if (uint64_t baked = 0; !force && !swatches && ReadModelLibraryHash(pack.Path, baked) && baked == sourceHash)
		{
			std::printf("ModelBaker: %s is up to date\n", pack.Path);
			return 0;
		}
		const auto start = std::chrono::steady_clock::now();
		ModelLibraryData library;
		library.SourceHash = sourceHash;
		GltfImporter importer(library);
		for (const ModelSource& source : pack.Sources)
		{
			std::string message;
			if (!importer.Import(assets / source.Path, source, message))
			{
				std::fprintf(stderr, "ModelBaker : error : %s\n", message.c_str());
				return 1;
			}
		}
		for (const std::string& warning : importer.GetWarnings())
			std::fprintf(stderr, "ModelBaker : warning : %s\n", warning.c_str());
		for (const std::string& report : importer.GetReports())
			std::printf("ModelBaker: %s\n", report.c_str());
		if (swatches)
		{
			ReportSwatches(library);
			return 0;
		}
		if (!SaveModelLibrary(pack.Path, library))
		{
			std::fprintf(stderr, "ModelBaker : error : can't write %s\n", pack.Path);
			return 1;
		}
		const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
		std::error_code sizeError;
		const uintmax_t bytes = std::filesystem::file_size(pack.Path, sizeError);
		std::printf("ModelBaker: baked %s, %zu meshes, %zu vertices, %zu triangles, %zu clips, %zu textures, %zu materials, %zu environments, %.1f MB in %.2f s\n",
			pack.Path, library.Meshes.size(), library.Vertices.size(), library.Indices.size() / 3, library.Clips.size(), library.Textures.size(), library.Materials.size(), library.Environments.size(),
			double(sizeError ? 0 : bytes) / 1048576.0, seconds);
		return 0;
	}
}

int main(int argc, char** argv)
{
	using namespace Game;
	bool force = false;
	bool swatches = false;
	bool list = false;
	std::string skyPath;
	std::vector<std::string> folders;
	for (int i = 1; i < argc; i++)
	{
		if (std::string_view(argv[i]) == "--force")
			force = true;
		else if (std::string_view(argv[i]) == "--swatches")
			swatches = true;
		else if (std::string_view(argv[i]) == "--list")
			list = true;
		else if (std::string_view(argv[i]) == "--export-sky" && i + 1 < argc)
			skyPath = std::filesystem::absolute(argv[++i]).string();
		else
			folders.push_back(argv[i]);
	}
	if (folders.size() != 2)
	{
		std::fprintf(stderr, "usage: ModelBaker <assets folder> <game folder> [--force] [--swatches] [--list] [--export-sky file.hdr]\n");
		return 1;
	}
	if (std::string message; !CheckSwatchTables(message))
	{
		std::fprintf(stderr, "ModelBaker : error : %s\n", message.c_str());
		return 1;
	}
	std::error_code error;
	const std::filesystem::path assets = std::filesystem::absolute(folders[0], error);
	if (error || !std::filesystem::is_directory(assets))
	{
		std::fprintf(stderr, "ModelBaker : error : can't open the assets folder %s\n", folders[0].c_str());
		return 1;
	}
	std::filesystem::current_path(folders[1], error);
	if (error)
	{
		std::fprintf(stderr, "ModelBaker : error : can't open the game folder %s\n", folders[1].c_str());
		return 1;
	}
	if (list)
	{
		for (const ModelPack& pack : ModelPacks)
			ListPack(pack);
		return 0;
	}
	if (!skyPath.empty())
	{
		for (const ModelPack& pack : ModelPacks)
			for (const ModelSource& source : pack.Sources)
			{
				if (source.Kind != ModelSourceKind::Environment)
					continue;
				ModelEnvironment environment;
				SkyImage sky;
				std::string report;
				std::string message;
				if (!BakeEnvironment(assets / source.Path, "sky", source.TextureSize, environment, report, message, &sky) || !WriteSkyImage(skyPath, sky))
				{
					std::fprintf(stderr, "ModelBaker : error : can't export %s: %s\n", source.Path, message.c_str());
					return 1;
				}
				std::printf("ModelBaker: wrote %s without the sun (%ux%u), %s\n", skyPath.c_str(), sky.Width, sky.Height, report.c_str());
				return 0;
			}
		std::fprintf(stderr, "ModelBaker : error : no environment source to export\n");
		return 1;
	}
	InitTextureEncoders();
	for (const ModelPack& pack : ModelPacks)
	{
		if (swatches && !UsesSwatches(pack))
			continue;
		if (const int result = BakePack(assets, pack, force, swatches); result != 0)
			return result;
	}
	return 0;
}

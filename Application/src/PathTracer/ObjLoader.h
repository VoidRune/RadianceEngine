#pragma once
#include "Scene.h"
#include <filesystem>
#include <optional>
#include <vector>

struct ObjMaterial
{
	Material Material;
	std::filesystem::path DiffuseTexture;
};

struct ObjPart
{
	uint32_t FirstIndex = 0;
	uint32_t IndexCount = 0;
	int32_t MaterialIndex = -1;
};

struct ObjModel
{
	MeshData Mesh;
	std::vector<ObjPart> Parts;
	std::vector<ObjMaterial> Materials;
};

std::optional<ObjModel> LoadObj(const std::filesystem::path& path);

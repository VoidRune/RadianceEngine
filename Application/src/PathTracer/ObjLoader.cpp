#include "ObjLoader.h"
#include "RadianceEngine/Core/Log.h"
#include "tiny_obj_loader/tiny_obj_loader.h"
#include <map>
#include <unordered_map>

namespace
{
	struct IndexKey
	{
		int Position;
		int Normal;
		int UV;

		bool operator==(const IndexKey&) const = default;
	};

	struct IndexKeyHash
	{
		size_t operator()(const IndexKey& key) const
		{
			uint64_t hash = 14695981039346656037ULL;
			for (int value : { key.Position, key.Normal, key.UV })
				hash = (hash ^ uint32_t(value)) * 1099511628211ULL;
			return size_t(hash);
		}
	};

	std::string_view TrimNewlines(std::string_view text)
	{
		while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
			text.remove_suffix(1);
		return text;
	}

	void GenerateMissingNormals(MeshData& mesh, const std::vector<bool>& missing)
	{
		for (size_t i = 0; i + 2 < mesh.Indices.size(); i += 3)
		{
			const uint32_t a = mesh.Indices[i], b = mesh.Indices[i + 1], c = mesh.Indices[i + 2];
			const glm::vec3 faceNormal = glm::cross(mesh.Vertices[b].Position - mesh.Vertices[a].Position, mesh.Vertices[c].Position - mesh.Vertices[a].Position);
			for (uint32_t vertex : { a, b, c })
			{
				if (missing[vertex])
					mesh.Vertices[vertex].Normal += faceNormal;
			}
		}

		for (size_t i = 0; i < mesh.Vertices.size(); i++)
		{
			if (!missing[i])
				continue;
			const float length = glm::length(mesh.Vertices[i].Normal);
			mesh.Vertices[i].Normal = length > 0.0f ? mesh.Vertices[i].Normal / length : glm::vec3(0.0f, 1.0f, 0.0f);
		}
	}
}

std::optional<ObjModel> LoadObj(const std::filesystem::path& path)
{
	tinyobj::ObjReaderConfig config;
	config.triangulate = true;

	tinyobj::ObjReader reader;
	if (!reader.ParseFromFile(path.string(), config))
	{
		RDN_LOG_ERROR("Failed to load {}: {}", path.generic_string(), TrimNewlines(reader.Error()));
		return std::nullopt;
	}
	if (!reader.Warning().empty())
	{
		RDN_LOG_WARNING("{}: {}", path.generic_string(), TrimNewlines(reader.Warning()));
	}

	const tinyobj::attrib_t& attrib = reader.GetAttrib();
	ObjModel model;
	std::vector<bool> missingNormals;
	std::unordered_map<IndexKey, uint32_t, IndexKeyHash> vertexLookup;
	std::map<int, std::vector<uint32_t>> indicesByMaterial;

	for (const tinyobj::shape_t& shape : reader.GetShapes())
	{
		size_t faceOffset = 0;
		for (size_t face = 0; face < shape.mesh.num_face_vertices.size(); face++)
		{
			const size_t cornerCount = shape.mesh.num_face_vertices[face];
			faceOffset += cornerCount;
			if (cornerCount != 3)
				continue;

			std::vector<uint32_t>& indices = indicesByMaterial[shape.mesh.material_ids[face]];
			for (size_t corner = 0; corner < 3; corner++)
			{
				const tinyobj::index_t index = shape.mesh.indices[faceOffset - 3 + corner];
				const IndexKey key{ index.vertex_index, index.normal_index, index.texcoord_index };
				const auto [it, inserted] = vertexLookup.try_emplace(key, uint32_t(model.Mesh.Vertices.size()));
				if (inserted)
				{
					Vertex& vertex = model.Mesh.Vertices.emplace_back();
					vertex.Position = { attrib.vertices[3 * index.vertex_index], attrib.vertices[3 * index.vertex_index + 1], attrib.vertices[3 * index.vertex_index + 2] };
					if (index.normal_index >= 0)
						vertex.Normal = { attrib.normals[3 * index.normal_index], attrib.normals[3 * index.normal_index + 1], attrib.normals[3 * index.normal_index + 2] };
					if (index.texcoord_index >= 0)
						vertex.UV = { attrib.texcoords[2 * index.texcoord_index], 1.0f - attrib.texcoords[2 * index.texcoord_index + 1] };
					missingNormals.push_back(index.normal_index < 0);
				}
				indices.push_back(it->second);
			}
		}
	}

	for (auto& [materialIndex, indices] : indicesByMaterial)
	{
		model.Parts.push_back({ uint32_t(model.Mesh.Indices.size()), uint32_t(indices.size()), materialIndex });
		model.Mesh.Indices.insert(model.Mesh.Indices.end(), indices.begin(), indices.end());
	}
	GenerateMissingNormals(model.Mesh, missingNormals);

	for (const tinyobj::material_t& source : reader.GetMaterials())
	{
		ObjMaterial& material = model.Materials.emplace_back();
		material.Material.Color = { source.diffuse[0], source.diffuse[1], source.diffuse[2] };
		material.Material.Emission = { source.emission[0], source.emission[1], source.emission[2] };
		material.Material.Metallic = source.metallic;
		material.Material.Roughness = source.roughness;
		material.Material.Transmission = 1.0f - source.dissolve;
		if (!source.diffuse_texname.empty())
			material.DiffuseTexture = path.parent_path() / source.diffuse_texname;
	}
	return model;
}

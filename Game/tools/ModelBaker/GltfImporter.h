#pragma once
#include "Rendering/ModelData.h"
#include "Rendering/ModelSources.h"
#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tinygltf
{
	class Model;
	struct Skin;
	class Node;
}

namespace Game
{
	bool CheckSwatchTables(std::string& error);

	class GltfImporter
	{
	public:
		explicit GltfImporter(ModelLibraryData& output) : m_Output(output) {}

		bool Import(const std::filesystem::path& path, const ModelSource& source, std::string& error);
		const std::vector<std::string>& GetWarnings() const { return m_Warnings; }
		const std::vector<std::string>& GetReports() const { return m_Reports; }

	private:
		struct ScanMaterial
		{
			uint32_t Texture = NoTexture;
			uint32_t Surface = NoTexture;
			uint32_t Normal = NoTexture;
			glm::vec4 Color{ 1.0f };
			float Roughness = 1.0f;
			float Metallic = 0.0f;
			float AlphaCutoff = 0.0f;
			bool Skip = false;
		};

		bool LoadModel(const std::filesystem::path& path, tinygltf::Model& model, std::string& error);
		bool SetupSkeleton(const tinygltf::Model& model, const tinygltf::Skin& skin, std::string& error);
		bool CheckSkin(const tinygltf::Model& model, const tinygltf::Skin& skin, const std::string& file, std::string& error);
		bool ImportCharacter(const tinygltf::Model& model, const std::string& file, std::string& error);
		bool ImportAnimations(const tinygltf::Model& model, const std::string& file, std::string& error);
		bool ImportProp(const tinygltf::Model& model, const std::string& file, std::string& error);
		bool ImportScan(const std::filesystem::path& folder, const ModelSource& source, std::string& error);
		bool ImportMaterial(const std::filesystem::path& folder, const ModelSource& source, std::string& error);
		bool ImportEnvironment(const std::filesystem::path& path, const ModelSource& source, std::string& error);
		ScanMaterial BakeScanMaterial(const tinygltf::Model& model, int materialIndex, const std::filesystem::path& folder, const ModelSource& source);
		uint32_t StoreTexture(ModelTexture texture);
		uint32_t ImportTexture(const tinygltf::Model& model, int textureIndex, bool surface);
		uint32_t SwatchSurface(uint32_t texture);
		void ApplyMaterial(const tinygltf::Model& model, int materialIndex, ModelMesh& mesh);
		void AddMesh(ModelMesh mesh, std::vector<ModelVertex>& vertices, const std::vector<uint32_t>& indices);
		std::string UniqueMeshName(const std::string& name, const std::string& file);
		int32_t FindJoint(const std::string& name) const;

		ModelLibraryData& m_Output;
		std::unordered_map<uint64_t, uint32_t> m_TextureByHash;
		std::unordered_set<std::string> m_MeshNames;
		std::unordered_set<std::string> m_ClipNames;
		std::vector<std::string> m_Warnings;
		std::vector<std::string> m_Reports;
	};
}

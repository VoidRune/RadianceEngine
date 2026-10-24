#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Game
{
	inline constexpr uint32_t NoTexture = UINT32_MAX;
	inline constexpr uint32_t MaxModelJoints = 32;
	inline constexpr uint32_t NoSkeleton = UINT32_MAX;
	inline constexpr const char* SharedSkeletonName = "Rig_Medium";

	struct ModelVertex
	{
		glm::vec3 Position{ 0.0f };
		float U = 0.0f;
		glm::vec3 Normal{ 0.0f, 1.0f, 0.0f };
		float V = 0.0f;
		uint32_t Joints = 0;
		uint32_t Weights = 0;
		uint32_t Tangent = 0;
		uint32_t Color = 0xFFFFFFFFu;
	};
	static_assert(sizeof(ModelVertex) == 48);

	enum class TextureFormat : uint8_t
	{
		Rgba8,
		Rgba8Srgb,
		Bc7,
		Bc7Srgb,
		Bc5,
		Count,
	};

	struct ModelTexture
	{
		std::string Name;
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t MipCount = 1;
		TextureFormat Format = TextureFormat::Rgba8;
		bool Nearest = false;
		bool Repeat = false;
		std::vector<uint8_t> Pixels;
	};

	struct ModelMesh
	{
		std::string Name;
		uint32_t FirstIndex = 0;
		uint32_t IndexCount = 0;
		uint32_t Texture = NoTexture;
		uint32_t Surface = NoTexture;
		uint32_t Normal = NoTexture;
		glm::vec4 Color{ 1.0f };
		glm::vec3 Emissive{ 0.0f };
		glm::vec3 Emission{ 0.0f };
		float Roughness = 1.0f;
		float Metallic = 0.0f;
		float AlphaCutoff = 0.0f;
		glm::vec3 Min{ 0.0f };
		glm::vec3 Max{ 0.0f };
		uint32_t Skeleton = NoSkeleton;
		bool Skinned = false;
		bool DoubleSided = true;
		bool Water = false;
	};

	struct ModelMaterial
	{
		std::string Name;
		uint32_t Albedo = NoTexture;
		uint32_t Normal = NoTexture;
		uint32_t Surface = NoTexture;
		float Size = 1.0f;
	};

	struct ModelEnvironment
	{
		std::string Name;
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t MipCount = 0;
		std::vector<uint16_t> Pixels;
		std::array<glm::vec3, 9> Irradiance{};
		glm::vec3 SunDirection{ 0.0f, -1.0f, 0.0f };
		glm::vec3 SunIrradiance{ 0.0f };
	};

	struct JointPose
	{
		glm::vec3 Translation{ 0.0f };
		glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale{ 1.0f };
	};

	struct ModelSkeleton
	{
		std::string Name;
		std::vector<std::string> Names;
		std::vector<int32_t> Parents;
		std::vector<JointPose> Rest;
		std::vector<glm::mat4> InverseBind;

		uint32_t JointCount() const { return uint32_t(Names.size()); }
	};

	struct ModelClip
	{
		std::string Name;
		uint32_t Skeleton = 0;
		float FrameRate = 30.0f;
		uint32_t FrameCount = 0;
		std::vector<JointPose> Frames;

		float Duration() const { return FrameCount > 1 ? float(FrameCount - 1) / FrameRate : 0.0f; }
	};

	struct ModelLibraryData
	{
		uint64_t SourceHash = 0;
		std::vector<ModelSkeleton> Skeletons;
		std::vector<ModelTexture> Textures;
		std::vector<ModelVertex> Vertices;
		std::vector<uint32_t> Indices;
		std::vector<ModelMesh> Meshes;
		std::vector<ModelClip> Clips;
		std::vector<ModelMaterial> Materials;
		std::vector<ModelEnvironment> Environments;
	};

	uint64_t TextureByteSize(TextureFormat format, uint32_t width, uint32_t height, uint32_t mips);
	bool SaveModelLibrary(const char* path, const ModelLibraryData& data);
	bool LoadModelLibrary(const char* path, ModelLibraryData& data);
	bool ReadModelLibraryHash(const char* path, uint64_t& hash);
	void AppendModelLibrary(ModelLibraryData& target, ModelLibraryData&& pack);
}

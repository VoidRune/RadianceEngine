#include "ModelData.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <type_traits>

namespace Game
{
	namespace
	{
		constexpr uint32_t FileMagic = 0x4C444F4D;
		constexpr uint32_t FileVersion = 5;
		constexpr uint64_t MaxElements = 1ull << 28;
		constexpr uint32_t MaxTextures = 4096;
		constexpr uint32_t MaxTextureSize = 8192;

		class Writer
		{
		public:
			explicit Writer(std::ofstream& file) : m_File(file) {}

			template<typename T>
			void Value(const T& value)
			{
				static_assert(std::is_trivially_copyable_v<T>);
				m_File.write(reinterpret_cast<const char*>(&value), sizeof(value));
			}

			void String(const std::string& text)
			{
				Value(uint32_t(text.size()));
				m_File.write(text.data(), std::streamsize(text.size()));
			}

			template<typename T>
			void Array(const std::vector<T>& values)
			{
				static_assert(std::is_trivially_copyable_v<T>);
				Value(uint64_t(values.size()));
				m_File.write(reinterpret_cast<const char*>(values.data()), std::streamsize(values.size() * sizeof(T)));
			}

		private:
			std::ofstream& m_File;
		};

		class Reader
		{
		public:
			explicit Reader(std::ifstream& file) : m_File(file) {}

			template<typename T>
			bool Value(T& value)
			{
				static_assert(std::is_trivially_copyable_v<T>);
				return bool(m_File.read(reinterpret_cast<char*>(&value), sizeof(value)));
			}

			bool String(std::string& text)
			{
				uint32_t size = 0;
				if (!Value(size) || size > 4096)
					return false;
				text.resize(size);
				return bool(m_File.read(text.data(), std::streamsize(size)));
			}

			template<typename T>
			bool Array(std::vector<T>& values)
			{
				uint64_t count = 0;
				if (!Value(count) || count > MaxElements)
					return false;
				values.resize(size_t(count));
				return bool(m_File.read(reinterpret_cast<char*>(values.data()), std::streamsize(count * sizeof(T))));
			}

		private:
			std::ifstream& m_File;
		};

		bool IsBlockCompressed(TextureFormat format)
		{
			return format == TextureFormat::Bc7 || format == TextureFormat::Bc7Srgb || format == TextureFormat::Bc5;
		}

		bool ValidTexture(uint32_t index, size_t count)
		{
			return index == NoTexture || index < count;
		}

		void WriteTextures(Writer& out, const std::vector<ModelTexture>& textures)
		{
			out.Value(uint32_t(textures.size()));
			for (const ModelTexture& texture : textures)
			{
				out.String(texture.Name);
				out.Value(texture.Width);
				out.Value(texture.Height);
				out.Value(texture.MipCount);
				out.Value(texture.Format);
				out.Value(uint8_t((texture.Nearest ? 1 : 0) | (texture.Repeat ? 2 : 0)));
				out.Array(texture.Pixels);
			}
		}

		bool ReadTextures(Reader& in, std::vector<ModelTexture>& textures)
		{
			uint32_t count = 0;
			if (!in.Value(count) || count > MaxTextures)
				return false;
			textures.resize(count);
			for (ModelTexture& texture : textures)
			{
				uint8_t flags = 0;
				if (!in.String(texture.Name) || !in.Value(texture.Width) || !in.Value(texture.Height) || !in.Value(texture.MipCount) || !in.Value(texture.Format) || !in.Value(flags) || !in.Array(texture.Pixels))
					return false;
				if (texture.Format >= TextureFormat::Count || texture.Width == 0 || texture.Height == 0 || texture.Width > MaxTextureSize || texture.Height > MaxTextureSize || texture.MipCount == 0 || texture.MipCount > 14)
					return false;
				if (texture.Pixels.size() != TextureByteSize(texture.Format, texture.Width, texture.Height, texture.MipCount))
					return false;
				texture.Nearest = (flags & 1) != 0;
				texture.Repeat = (flags & 2) != 0;
			}
			return true;
		}

		uint64_t EnvironmentPixels(const ModelEnvironment& environment)
		{
			uint64_t total = 0;
			for (uint32_t mip = 0; mip < environment.MipCount; mip++)
				total += uint64_t(std::max(environment.Width >> mip, 1u)) * std::max(environment.Height >> mip, 1u) * 4;
			return total;
		}
	}

	uint64_t TextureByteSize(TextureFormat format, uint32_t width, uint32_t height, uint32_t mips)
	{
		uint64_t total = 0;
		for (uint32_t mip = 0; mip < mips; mip++)
		{
			const uint64_t w = std::max(width >> mip, 1u);
			const uint64_t h = std::max(height >> mip, 1u);
			total += IsBlockCompressed(format) ? ((w + 3) / 4) * ((h + 3) / 4) * 16 : w * h * 4;
		}
		return total;
	}

	bool SaveModelLibrary(const char* path, const ModelLibraryData& data)
	{
		const std::filesystem::path target(path);
		std::error_code error;
		if (target.has_parent_path())
			std::filesystem::create_directories(target.parent_path(), error);
		const std::filesystem::path temporary = target.string() + ".tmp";
		{
			std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
			if (!file)
				return false;
			Writer out(file);
			out.Value(FileMagic);
			out.Value(FileVersion);
			out.Value(data.SourceHash);

			out.Value(uint32_t(data.Skeletons.size()));
			for (const ModelSkeleton& skeleton : data.Skeletons)
			{
				out.String(skeleton.Name);
				out.Value(uint32_t(skeleton.Names.size()));
				for (const std::string& name : skeleton.Names)
					out.String(name);
				out.Array(skeleton.Parents);
				out.Array(skeleton.Rest);
				out.Array(skeleton.InverseBind);
			}

			WriteTextures(out, data.Textures);

			out.Array(data.Vertices);
			out.Array(data.Indices);

			out.Value(uint32_t(data.Meshes.size()));
			for (const ModelMesh& mesh : data.Meshes)
			{
				out.String(mesh.Name);
				out.Value(mesh.FirstIndex);
				out.Value(mesh.IndexCount);
				out.Value(mesh.Texture);
				out.Value(mesh.Surface);
				out.Value(mesh.Normal);
				out.Value(mesh.Color);
				out.Value(mesh.Emissive);
				out.Value(mesh.Emission);
				out.Value(mesh.Roughness);
				out.Value(mesh.Metallic);
				out.Value(mesh.AlphaCutoff);
				out.Value(mesh.Min);
				out.Value(mesh.Max);
				out.Value(uint8_t((mesh.Skinned ? 1 : 0) | (mesh.DoubleSided ? 0 : 2) | (mesh.Water ? 4 : 0)));
				out.Value(mesh.Skeleton);
			}

			out.Value(uint32_t(data.Clips.size()));
			for (const ModelClip& clip : data.Clips)
			{
				out.String(clip.Name);
				out.Value(clip.Skeleton);
				out.Value(clip.FrameRate);
				out.Value(clip.FrameCount);
				out.Array(clip.Frames);
			}

			out.Value(uint32_t(data.Materials.size()));
			for (const ModelMaterial& material : data.Materials)
			{
				out.String(material.Name);
				out.Value(material.Albedo);
				out.Value(material.Normal);
				out.Value(material.Surface);
				out.Value(material.Size);
			}

			out.Value(uint32_t(data.Environments.size()));
			for (const ModelEnvironment& environment : data.Environments)
			{
				out.String(environment.Name);
				out.Value(environment.Width);
				out.Value(environment.Height);
				out.Value(environment.MipCount);
				out.Array(environment.Pixels);
				out.Value(environment.Irradiance);
				out.Value(environment.SunDirection);
				out.Value(environment.SunIrradiance);
			}
			if (!file)
				return false;
		}
		std::filesystem::rename(temporary, target, error);
		return !error;
	}

	bool ReadModelLibraryHash(const char* path, uint64_t& hash)
	{
		std::ifstream file(path, std::ios::binary);
		Reader in(file);
		uint32_t magic = 0;
		uint32_t version = 0;
		return file && in.Value(magic) && magic == FileMagic && in.Value(version) && version == FileVersion && in.Value(hash);
	}

	bool LoadModelLibrary(const char* path, ModelLibraryData& data)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
			return false;
		Reader in(file);
		uint32_t magic = 0;
		uint32_t version = 0;
		if (!in.Value(magic) || magic != FileMagic || !in.Value(version) || version != FileVersion || !in.Value(data.SourceHash))
			return false;

		uint32_t skeletons = 0;
		if (!in.Value(skeletons) || skeletons > 64)
			return false;
		data.Skeletons.resize(skeletons);
		for (ModelSkeleton& skeleton : data.Skeletons)
		{
			uint32_t joints = 0;
			if (!in.String(skeleton.Name) || !in.Value(joints) || joints > MaxModelJoints)
				return false;
			skeleton.Names.resize(joints);
			for (std::string& name : skeleton.Names)
				if (!in.String(name))
					return false;
			if (!in.Array(skeleton.Parents) || !in.Array(skeleton.Rest) || !in.Array(skeleton.InverseBind))
				return false;
			if (skeleton.Parents.size() != joints || skeleton.Rest.size() != joints || skeleton.InverseBind.size() != joints)
				return false;
			for (uint32_t joint = 0; joint < joints; joint++)
				if (skeleton.Parents[joint] >= int32_t(joint))
					return false;
		}

		if (!ReadTextures(in, data.Textures))
			return false;
		if (!in.Array(data.Vertices) || !in.Array(data.Indices))
			return false;

		uint32_t meshes = 0;
		if (!in.Value(meshes) || meshes > 65536)
			return false;
		data.Meshes.resize(meshes);
		const size_t textures = data.Textures.size();
		for (ModelMesh& mesh : data.Meshes)
		{
			uint8_t flags = 0;
			if (!in.String(mesh.Name) || !in.Value(mesh.FirstIndex) || !in.Value(mesh.IndexCount) || !in.Value(mesh.Texture) || !in.Value(mesh.Surface) || !in.Value(mesh.Normal) || !in.Value(mesh.Color) || !in.Value(mesh.Emissive)
				|| !in.Value(mesh.Emission) || !in.Value(mesh.Roughness) || !in.Value(mesh.Metallic) || !in.Value(mesh.AlphaCutoff) || !in.Value(mesh.Min) || !in.Value(mesh.Max) || !in.Value(flags) || !in.Value(mesh.Skeleton))
				return false;
			mesh.Skinned = (flags & 1) != 0;
			mesh.DoubleSided = (flags & 2) == 0;
			mesh.Water = (flags & 4) != 0;
			if (uint64_t(mesh.FirstIndex) + mesh.IndexCount > data.Indices.size() || !ValidTexture(mesh.Texture, textures) || !ValidTexture(mesh.Surface, textures) || !ValidTexture(mesh.Normal, textures))
				return false;
			if (mesh.Skinned ? mesh.Skeleton >= skeletons : mesh.Skeleton != NoSkeleton)
				return false;
		}

		uint32_t clips = 0;
		if (!in.Value(clips) || clips > 4096)
			return false;
		data.Clips.resize(clips);
		for (ModelClip& clip : data.Clips)
			if (!in.String(clip.Name) || !in.Value(clip.Skeleton) || clip.Skeleton >= skeletons || !in.Value(clip.FrameRate) || !in.Value(clip.FrameCount) || !in.Array(clip.Frames)
				|| clip.Frames.size() != size_t(clip.FrameCount) * data.Skeletons[clip.Skeleton].JointCount() || clip.FrameCount == 0 || clip.FrameRate <= 0.0f)
				return false;

		uint32_t materials = 0;
		if (!in.Value(materials) || materials > 4096)
			return false;
		data.Materials.resize(materials);
		for (ModelMaterial& material : data.Materials)
			if (!in.String(material.Name) || !in.Value(material.Albedo) || !in.Value(material.Normal) || !in.Value(material.Surface) || !in.Value(material.Size)
				|| !ValidTexture(material.Albedo, textures) || !ValidTexture(material.Normal, textures) || !ValidTexture(material.Surface, textures) || !(material.Size > 0.0f))
				return false;

		uint32_t environments = 0;
		if (!in.Value(environments) || environments > 64)
			return false;
		data.Environments.resize(environments);
		for (ModelEnvironment& environment : data.Environments)
			if (!in.String(environment.Name) || !in.Value(environment.Width) || !in.Value(environment.Height) || !in.Value(environment.MipCount) || !in.Array(environment.Pixels)
				|| !in.Value(environment.Irradiance) || !in.Value(environment.SunDirection) || !in.Value(environment.SunIrradiance)
				|| environment.Width == 0 || environment.Height == 0 || environment.Width > MaxTextureSize || environment.MipCount == 0 || environment.MipCount > 14 || environment.Pixels.size() != EnvironmentPixels(environment))
				return false;

		for (const uint32_t index : data.Indices)
			if (index >= data.Vertices.size())
				return false;
		return true;
	}

	void AppendModelLibrary(ModelLibraryData& target, ModelLibraryData&& pack)
	{
		const uint32_t textureBase = uint32_t(target.Textures.size());
		const uint32_t vertexBase = uint32_t(target.Vertices.size());
		const uint32_t indexBase = uint32_t(target.Indices.size());
		const auto offset = [&](uint32_t index) { return index == NoTexture ? NoTexture : index + textureBase; };
		const uint32_t skeletonBase = uint32_t(target.Skeletons.size());
		std::move(pack.Skeletons.begin(), pack.Skeletons.end(), std::back_inserter(target.Skeletons));
		std::move(pack.Textures.begin(), pack.Textures.end(), std::back_inserter(target.Textures));
		target.Vertices.insert(target.Vertices.end(), pack.Vertices.begin(), pack.Vertices.end());
		for (const uint32_t index : pack.Indices)
			target.Indices.push_back(index + vertexBase);
		for (ModelMesh& mesh : pack.Meshes)
		{
			mesh.FirstIndex += indexBase;
			mesh.Texture = offset(mesh.Texture);
			mesh.Surface = offset(mesh.Surface);
			mesh.Normal = offset(mesh.Normal);
			if (mesh.Skeleton != NoSkeleton)
				mesh.Skeleton += skeletonBase;
			target.Meshes.push_back(std::move(mesh));
		}
		for (ModelClip& clip : pack.Clips)
		{
			clip.Skeleton += skeletonBase;
			target.Clips.push_back(std::move(clip));
		}
		for (ModelMaterial& material : pack.Materials)
		{
			material.Albedo = offset(material.Albedo);
			material.Normal = offset(material.Normal);
			material.Surface = offset(material.Surface);
			target.Materials.push_back(std::move(material));
		}
		std::move(pack.Environments.begin(), pack.Environments.end(), std::back_inserter(target.Environments));
	}
}

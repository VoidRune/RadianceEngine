#pragma once
#include "Rendering/ModelData.h"
#include "Rendering/ModelSources.h"
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Game
{
	class ModelLibrary
	{
	public:
		bool Load(std::span<const ModelPack> packs, std::string& error);

		const ModelLibraryData& GetData() const { return m_Data; }
		uint32_t GetSkeletonCount() const { return uint32_t(m_Data.Skeletons.size()); }
		uint32_t GetSharedSkeleton() const { return m_Shared; }
		const ModelSkeleton& GetSkeleton(uint32_t skeleton) const { return m_Data.Skeletons[skeleton]; }
		const ModelMesh& GetMesh(uint32_t index) const { return m_Data.Meshes[index]; }
		const ModelClip& GetClip(uint32_t index) const { return m_Data.Clips[index]; }
		float GetStrikeTime(uint32_t clip) const { return clip < m_StrikeTimes.size() ? m_StrikeTimes[clip] : 0.0f; }
		float GetStrideSpeed(uint32_t clip) const { return clip < m_StrideSpeeds.size() ? m_StrideSpeeds[clip] : 0.0f; }

		std::optional<uint32_t> FindMesh(std::string_view name) const;
		std::optional<uint32_t> FindSkeleton(std::string_view name) const;
		std::optional<uint32_t> FindClip(std::string_view name, uint32_t skeleton) const;
		std::optional<uint32_t> FindJoint(std::string_view name, uint32_t skeleton) const;
		std::optional<uint32_t> FindMaterial(std::string_view name) const;
		std::optional<uint32_t> FindEnvironment(std::string_view name) const;

	private:
		static std::string ClipKey(std::string_view name, uint32_t skeleton);
		void RetargetClips();
		void MeasureClips();
		void ExtractRootMotion(size_t authored);

		ModelLibraryData m_Data;
		std::unordered_map<std::string, uint32_t> m_Meshes;
		std::unordered_map<std::string, uint32_t> m_Clips;
		std::vector<float> m_StrikeTimes;
		std::vector<float> m_StrideSpeeds;
		uint32_t m_Shared = 0;
	};
}

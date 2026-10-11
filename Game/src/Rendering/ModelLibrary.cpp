#include "ModelLibrary.h"
#include "Rendering/Animation.h"
#include <RadianceEngine/Core/Log.h>
#include <algorithm>

namespace Game
{
	bool ModelLibrary::Load(std::span<const ModelPack> packs, std::string& error)
	{
		m_Data = {};
		for (const ModelPack& pack : packs)
		{
			ModelLibraryData data;
			if (!LoadModelLibrary(pack.Path, data))
			{
				if (pack.Required)
				{
					error = std::string(pack.Path) + " is missing or damaged, rebuild the game to bake it";
					return false;
				}
				RDN_LOG_WARNING("{} is missing or damaged, its models won't be drawn", pack.Path);
				continue;
			}
			AppendModelLibrary(m_Data, std::move(data));
		}
		if (m_Data.Skeleton.JointCount() == 0)
		{
			error = "the model library has no skeleton";
			return false;
		}
		m_Meshes.clear();
		m_Clips.clear();
		for (uint32_t i = 0; i < m_Data.Meshes.size(); i++)
			m_Meshes.emplace(m_Data.Meshes[i].Name, i);
		for (uint32_t i = 0; i < m_Data.Clips.size(); i++)
			m_Clips.emplace(m_Data.Clips[i].Name, i);
		MeasureClips();
		return true;
	}

	std::optional<uint32_t> ModelLibrary::FindMesh(std::string_view name) const
	{
		const auto it = m_Meshes.find(std::string(name));
		return it != m_Meshes.end() ? std::optional<uint32_t>(it->second) : std::nullopt;
	}

	std::optional<uint32_t> ModelLibrary::FindClip(std::string_view name) const
	{
		const auto it = m_Clips.find(std::string(name));
		return it != m_Clips.end() ? std::optional<uint32_t>(it->second) : std::nullopt;
	}

	std::optional<uint32_t> ModelLibrary::FindJoint(std::string_view name) const
	{
		const std::vector<std::string>& names = m_Data.Skeleton.Names;
		const auto it = std::find(names.begin(), names.end(), name);
		return it != names.end() ? std::optional<uint32_t>(uint32_t(it - names.begin())) : std::nullopt;
	}

	std::optional<uint32_t> ModelLibrary::FindMaterial(std::string_view name) const
	{
		for (uint32_t i = 0; i < m_Data.Materials.size(); i++)
			if (m_Data.Materials[i].Name == name)
				return i;
		return std::nullopt;
	}

	std::optional<uint32_t> ModelLibrary::FindEnvironment(std::string_view name) const
	{
		for (uint32_t i = 0; i < m_Data.Environments.size(); i++)
			if (m_Data.Environments[i].Name == name)
				return i;
		return std::nullopt;
	}

	void ModelLibrary::MeasureClips()
	{
		m_StrikeTimes.assign(m_Data.Clips.size(), 0.0f);
		m_StrideSpeeds.assign(m_Data.Clips.size(), 0.0f);
		const std::optional<uint32_t> hand = FindJoint("handslot.r");
		const std::optional<uint32_t> foot = FindJoint("foot.l");
		if (!hand || !foot)
			return;
		const ModelSkeleton& skeleton = m_Data.Skeleton;
		Pose pose;
		RestPose(skeleton, pose);
		JointMatrices globals;
		for (size_t i = 0; i < m_Data.Clips.size(); i++)
		{
			const ModelClip& clip = m_Data.Clips[i];
			float fastest = -1.0f;
			glm::vec3 previous(0.0f);
			glm::vec3 low(1e9f);
			glm::vec3 high(-1e9f);
			for (uint32_t frame = 0; frame < clip.FrameCount; frame++)
			{
				const float time = float(frame) / clip.FrameRate;
				SampleClip(clip, skeleton.JointCount(), time, false, pose);
				ComputeGlobals(skeleton, pose, globals);
				const glm::vec3 position(globals[*hand][3]);
				low = glm::min(low, glm::vec3(globals[*foot][3]));
				high = glm::max(high, glm::vec3(globals[*foot][3]));
				const float speed = frame > 0 ? glm::length(position - previous) : 0.0f;
				if (speed > fastest)
				{
					fastest = speed;
					m_StrikeTimes[i] = time;
				}
				previous = position;
			}
			const glm::vec3 reach = high - low;
			if (clip.Duration() > 0.0f)
				m_StrideSpeeds[i] = 2.0f * std::max(reach.x, reach.z) / clip.Duration();
		}
	}
}

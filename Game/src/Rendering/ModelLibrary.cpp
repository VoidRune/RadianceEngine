#include "ModelLibrary.h"
#include "Rendering/Animation.h"
#include <RadianceEngine/Core/Log.h>
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr float ArmTuck = 0.35f;
		constexpr float ContactHeight = 0.03f;
		constexpr uint32_t MinimumContacts = 4;
		constexpr float RootTravel = 0.05f;
		constexpr float ContactBand = 0.07f;
		constexpr float ContactSlip = 0.4f;
		constexpr float MovingPace = 0.3f;
	}

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
		const std::optional<uint32_t> shared = FindSkeleton(SharedSkeletonName);
		if (!shared || m_Data.Skeletons[*shared].JointCount() == 0)
		{
			error = std::string("the model library has no ") + SharedSkeletonName + " skeleton";
			return false;
		}
		m_Shared = *shared;
		m_Meshes.clear();
		m_Clips.clear();
		for (uint32_t i = 0; i < m_Data.Meshes.size(); i++)
			m_Meshes.emplace(m_Data.Meshes[i].Name, i);
		for (uint32_t i = 0; i < m_Data.Clips.size(); i++)
			m_Clips.emplace(ClipKey(m_Data.Clips[i].Name, m_Data.Clips[i].Skeleton), i);
		const size_t authored = m_Data.Clips.size();
		RetargetClips();
		MeasureClips();
		ExtractRootMotion(authored);
		MeasureGaits();
		return true;
	}

	std::string ModelLibrary::ClipKey(std::string_view name, uint32_t skeleton)
	{
		return std::to_string(skeleton) + "/" + std::string(name);
	}

	void ModelLibrary::RetargetClips()
	{
		const uint32_t count = uint32_t(m_Data.Clips.size());
		for (uint32_t skeleton = 0; skeleton < m_Data.Skeletons.size(); skeleton++)
		{
			if (skeleton == m_Shared)
				continue;
			const std::vector<std::string>& names = m_Data.Skeletons[skeleton].Names;
			const std::vector<std::string>& source = m_Data.Skeletons[m_Shared].Names;
			if (std::find_first_of(names.begin(), names.end(), source.begin(), source.end()) == names.end())
				continue;
			uint32_t added = 0;
			for (uint32_t i = 0; i < count; i++)
			{
				if (m_Data.Clips[i].Skeleton != m_Shared || FindClip(m_Data.Clips[i].Name, skeleton))
					continue;
				ModelClip clip = RetargetClip(m_Data.Clips[i], m_Data.Skeletons[m_Shared], m_Data.Skeletons[skeleton], ArmTuck);
				clip.Skeleton = skeleton;
				m_Clips.emplace(ClipKey(clip.Name, skeleton), uint32_t(m_Data.Clips.size()));
				m_Data.Clips.push_back(std::move(clip));
				added++;
			}
			if (added > 0)
				RDN_LOG("Retargeted {} clips onto the {} skeleton", added, m_Data.Skeletons[skeleton].Name);
		}
	}

	std::optional<uint32_t> ModelLibrary::FindMesh(std::string_view name) const
	{
		const auto it = m_Meshes.find(std::string(name));
		return it != m_Meshes.end() ? std::optional<uint32_t>(it->second) : std::nullopt;
	}

	std::optional<uint32_t> ModelLibrary::FindSkeleton(std::string_view name) const
	{
		for (uint32_t i = 0; i < m_Data.Skeletons.size(); i++)
			if (m_Data.Skeletons[i].Name == name)
				return i;
		return std::nullopt;
	}

	std::optional<uint32_t> ModelLibrary::FindClip(std::string_view name, uint32_t skeleton) const
	{
		const auto it = m_Clips.find(ClipKey(name, skeleton));
		return it != m_Clips.end() ? std::optional<uint32_t>(it->second) : std::nullopt;
	}

	std::optional<uint32_t> ModelLibrary::FindJoint(std::string_view name, uint32_t skeleton) const
	{
		if (skeleton >= m_Data.Skeletons.size())
			return std::nullopt;
		const std::vector<std::string>& names = m_Data.Skeletons[skeleton].Names;
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

	void ModelLibrary::ExtractRootMotion(size_t authored)
	{
		for (size_t i = 0; i < std::min(authored, m_Data.Clips.size()); i++)
		{
			ModelClip& clip = m_Data.Clips[i];
			if (clip.Skeleton == m_Shared || clip.FrameCount < 2 || clip.Duration() <= 0.0f)
				continue;
			const ModelSkeleton& skeleton = m_Data.Skeletons[clip.Skeleton];
			const uint32_t joints = std::min(skeleton.JointCount(), MaxModelJoints);
			for (uint32_t joint = 0; joint < joints; joint++)
			{
				if (skeleton.Parents[joint] >= 0)
					continue;
				const glm::vec3 first = clip.Frames[joint].Translation;
				const glm::vec3 last = clip.Frames[size_t(clip.FrameCount - 1) * joints + joint].Translation;
				const float travel = glm::length(glm::vec2(last.x - first.x, last.z - first.z));
				if (travel < RootTravel)
					continue;
				m_StrideSpeeds[i] = travel / clip.Duration();
				for (uint32_t frame = 0; frame < clip.FrameCount; frame++)
					clip.Frames[size_t(frame) * joints + joint].Translation = first;
			}
		}
	}

	void ModelLibrary::MeasureClips()
	{
		m_StrikeTimes.assign(m_Data.Clips.size(), 0.0f);
		m_StrideSpeeds.assign(m_Data.Clips.size(), 0.0f);
		JointMatrices globals;
		for (size_t i = 0; i < m_Data.Clips.size(); i++)
		{
			const ModelClip& clip = m_Data.Clips[i];
			const std::optional<uint32_t> hand = FindJoint("handslot.r", clip.Skeleton);
			const std::optional<uint32_t> feet[] = { FindJoint("foot.l", clip.Skeleton), FindJoint("foot.r", clip.Skeleton) };
			if (!hand || !feet[0])
				continue;
			const ModelSkeleton& skeleton = m_Data.Skeletons[clip.Skeleton];
			Pose pose;
			RestPose(skeleton, pose);
			float fastest = -1.0f;
			glm::vec3 previous(0.0f);
			glm::vec3 low(1e9f);
			glm::vec3 high(-1e9f);
			std::vector<glm::vec3> steps[2];
			for (uint32_t frame = 0; frame < clip.FrameCount; frame++)
			{
				const float time = float(frame) / clip.FrameRate;
				SampleClip(clip, skeleton.JointCount(), time, false, pose);
				ComputeGlobals(skeleton, pose, globals);
				const glm::vec3 position(globals[*hand][3]);
				low = glm::min(low, glm::vec3(globals[*feet[0]][3]));
				high = glm::max(high, glm::vec3(globals[*feet[0]][3]));
				for (uint32_t side = 0; side < 2; side++)
					if (feet[side])
						steps[side].push_back(glm::vec3(globals[*feet[side]][3]));
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
			float planted = 0.0f;
			uint32_t samples = 0;
			for (const std::vector<glm::vec3>& track : steps)
			{
				if (track.size() < 3)
					continue;
				float floor = 1e9f;
				for (const glm::vec3& point : track)
					floor = std::min(floor, point.y);
				for (size_t frame = 1; frame < track.size(); frame++)
					if (track[frame].y < floor + ContactHeight && track[frame - 1].y < floor + ContactHeight)
					{
						planted += glm::length(glm::vec2(track[frame].x - track[frame - 1].x, track[frame].z - track[frame - 1].z)) * clip.FrameRate;
						samples++;
					}
			}
			if (samples >= MinimumContacts && planted / float(samples) > 0.2f)
				m_StrideSpeeds[i] = planted / float(samples);
		}
	}

	bool ModelLibrary::IsPlanted(uint32_t clip, float time, uint32_t side) const
	{
		if (clip >= m_Gaits.size() || m_Gaits[clip].Contacts.empty())
			return true;
		const std::vector<uint8_t>& contacts = m_Gaits[clip].Contacts;
		const float duration = m_Data.Clips[clip].Duration();
		if (duration > 0.0f)
			time -= duration * std::floor(time / duration);
		const size_t frame = std::min(size_t(time * m_Data.Clips[clip].FrameRate + 0.5f), contacts.size() - 1);
		return (contacts[frame] >> side & 1u) != 0;
	}

	void ModelLibrary::MeasureGaits()
	{
		m_Gaits.assign(m_Data.Clips.size(), {});
		JointMatrices globals;
		for (size_t i = 0; i < m_Data.Clips.size(); i++)
		{
			const ModelClip& clip = m_Data.Clips[i];
			const std::optional<uint32_t> feet[] = { FindJoint("foot.l", clip.Skeleton), FindJoint("foot.r", clip.Skeleton) };
			const std::optional<uint32_t> hips = FindJoint("hips", clip.Skeleton);
			if (!feet[0] || !feet[1] || !hips || clip.FrameCount < 2)
				continue;
			const ModelSkeleton& skeleton = m_Data.Skeletons[clip.Skeleton];
			Pose pose;
			RestPose(skeleton, pose);
			ComputeGlobals(skeleton, pose, globals);
			const float band = ContactBand * std::max(globals[*hips][3].y, 0.1f);
			std::vector<glm::vec3> tracks[2];
			for (uint32_t frame = 0; frame < clip.FrameCount; frame++)
			{
				SampleClip(clip, skeleton.JointCount(), float(frame) / clip.FrameRate, false, pose);
				ComputeGlobals(skeleton, pose, globals);
				for (uint32_t side = 0; side < 2; side++)
					tracks[side].push_back(glm::vec3(globals[*feet[side]][3]));
			}
			const uint32_t cycle = clip.FrameCount - 1;
			const auto velocityOf = [&](uint32_t side, uint32_t frame)
			{
				const glm::vec3 step = tracks[side][(frame + 1) % cycle] - tracks[side][(frame + cycle - 1) % cycle];
				return glm::vec2(step.x, step.z) * (0.5f * clip.FrameRate);
			};
			std::vector<uint8_t> low(clip.FrameCount, 0);
			glm::vec2 travel(0.0f);
			uint32_t samples = 0;
			for (uint32_t side = 0; side < 2; side++)
			{
				float floor = 1e9f;
				for (const glm::vec3& point : tracks[side])
					floor = std::min(floor, point.y);
				for (uint32_t frame = 0; frame < clip.FrameCount; frame++)
					if (tracks[side][frame].y < floor + band)
					{
						low[frame] |= uint8_t(1u << side);
						travel -= velocityOf(side, frame);
						samples++;
					}
			}
			const float drift = glm::length(travel);
			const float pace = m_StrideSpeeds[i] > MovingPace ? m_StrideSpeeds[i] : drift / float(std::max(samples, 1u));
			travel = drift > 1e-5f ? travel * (pace / drift) : glm::vec2(0.0f);
			Gait& gait = m_Gaits[i];
			gait.Contacts = low;
			if (pace > MovingPace && cycle >= 4)
			{
				float planted = 0.0f;
				uint32_t contacts = 0;
				for (uint32_t frame = 0; frame < clip.FrameCount; frame++)
					for (uint32_t side = 0; side < 2; side++)
					{
						if (!(low[frame] >> side & 1u))
							continue;
						const glm::vec2 velocity = velocityOf(side, frame);
						if (glm::length(velocity + travel) > ContactSlip * pace)
							gait.Contacts[frame] &= uint8_t(~(1u << side));
						else
						{
							planted += glm::length(velocity);
							contacts++;
						}
					}
				if (contacts >= MinimumContacts)
					m_StrideSpeeds[i] = planted / float(contacts);
			}
			for (uint32_t frame = 1; frame < clip.FrameCount; frame++)
				if ((gait.Contacts[frame] & 1u) && !(gait.Contacts[frame - 1] & 1u))
				{
					gait.Sync = float(frame) / float(clip.FrameCount - 1);
					break;
				}
		}
	}
}

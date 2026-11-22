#include "TreeArt.h"
#include "Rendering/LevelRenderer.h"
#include "Rendering/ModelLibrary.h"
#include "Rendering/WorldRenderer.h"
#include <GameShared/Content/Props.h>
#include <GameShared/World/Level.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <string>

namespace Game
{
	namespace
	{
		constexpr glm::vec3 Crown{ -0.08f, 8.1f, -0.13f };
		constexpr glm::vec2 CrownRadius{ 4.6f, 3.6f };
		constexpr glm::vec3 WindHeading{ 0.83f, 0.0f, 0.55f };
		constexpr float TrunkSway = 0.008f;
		constexpr float CrownSway = 0.022f;
		constexpr float ClusterSway = 0.06f;
		constexpr float ClusterFlap = 0.11f;
		constexpr float ClusterTwist = 0.07f;
		constexpr float GustSpeed = 3.5f;

		glm::mat4 Turn(float yaw)
		{
			const float c = std::cos(yaw);
			const float s = std::sin(yaw);
			glm::mat4 m(1.0f);
			m[0] = glm::vec4(c, 0.0f, s, 0.0f);
			m[2] = glm::vec4(-s, 0.0f, c, 0.0f);
			return m;
		}

		glm::mat4 Pivot(const glm::vec3& point, const glm::vec3& axis, float angle)
		{
			if (std::abs(angle) < 1e-6f)
				return glm::mat4(1.0f);
			return glm::translate(glm::mat4(1.0f), point) * glm::rotate(glm::mat4(1.0f), angle, axis) * glm::translate(glm::mat4(1.0f), -point);
		}

		float Hash(float n)
		{
			const float s = std::sin(n * 12.9898f) * 43758.5453f;
			return s - std::floor(s);
		}

		float Noise(float t)
		{
			const float i = std::floor(t);
			const float f = t - i;
			const float u = f * f * (3.0f - 2.0f * f);
			return std::lerp(Hash(i), Hash(i + 1.0f), u) * 2.0f - 1.0f;
		}

		float Turbulence(float t, float seed)
		{
			return 0.6f * Noise(t + seed) + 0.28f * Noise(t * 2.13f + seed * 1.7f + 11.0f) + 0.12f * Noise(t * 4.37f + seed * 0.3f + 23.0f);
		}

		float Gust(float time, float seed)
		{
			const float swell = 0.5f + 0.5f * Turbulence(time * 0.16f, seed);
			return 0.25f + 0.75f * swell * swell;
		}

		glm::mat4 Placement(const PlacedProp& prop)
		{
			return glm::translate(glm::mat4(1.0f), glm::vec3(prop.Position.x, prop.Base, prop.Position.y)) * Turn(prop.Yaw) * Turn(Pi);
		}
	}

	glm::vec3 TreeLeafSpawn(const PlacedProp& tree, uint32_t stream, float time)
	{
		const float angle = time * (0.37f + 0.11f * float(stream)) + float(stream) * 2.1f;
		const float reach = 0.55f + 0.35f * std::sin(time * 0.29f + float(stream) * 1.3f);
		const glm::vec3 local(Crown.x + std::cos(angle) * CrownRadius.x * reach, Crown.y - 1.4f + 0.8f * std::sin(time * 0.41f + float(stream)), Crown.z + std::sin(angle) * CrownRadius.y * reach);
		return glm::vec3(Placement(tree) * glm::vec4(local, 1.0f));
	}

	void TreeArt::Clear()
	{
		m_Trees.clear();
	}

	void TreeArt::Build(const WorldRenderer& renderer, const Level& level)
	{
		Clear();
		const ModelLibrary& models = renderer.GetModels();
		const auto collect = [&](std::string_view name)
		{
			std::vector<uint32_t> found;
			if (const std::optional<uint32_t> first = models.FindMesh(name))
			{
				found.push_back(*first);
				for (uint32_t extra = 1;; extra++)
				{
					const std::optional<uint32_t> more = models.FindMesh(std::string(name) + "." + std::to_string(extra));
					if (!more)
						break;
					found.push_back(*more);
				}
			}
			return found;
		};
		m_Bark = collect("AutumnTree/Bark");
		m_Leaves = collect("AutumnTree/Leaves");
		m_Joints.clear();
		const std::optional<uint32_t> skeleton = models.FindSkeleton("AutumnTree");
		if (!skeleton || (m_Bark.empty() && m_Leaves.empty()))
			return;
		const ModelSkeleton& rig = models.GetSkeleton(*skeleton);
		const uint32_t count = std::min(rig.JointCount(), MaxModelJoints);
		m_Joints.resize(count);
		for (uint32_t i = 0; i < count; i++)
		{
			Joint& joint = m_Joints[i];
			const std::string& name = rig.Names[i];
			joint.Parent = rig.Parents[i];
			joint.Rest = glm::vec3(glm::inverse(rig.InverseBind[i])[3]);
			joint.Kind = name == "trunk" ? Part::Trunk : name == "crown" ? Part::Crown : name.starts_with("leaf") ? Part::Cluster : Part::Root;
			joint.Seed = float(i) * 2.399f;
			joint.Reach = glm::length(glm::vec2(joint.Rest.x - Crown.x, joint.Rest.z - Crown.z));
		}
		for (const PlacedProp& prop : level.Props)
			if (prop.Id == PropId::AutumnTree)
				m_Trees.push_back({ Placement(prop), glm::vec2(prop.Position.x, prop.Position.y), prop.Base, prop.Position.x * 0.37f + prop.Position.y * 0.61f });
	}

	void TreeArt::Draw(WorldRenderer& renderer, const LevelView& view) const
	{
		if (m_Trees.empty() || m_Joints.empty())
			return;
		for (const Tree& tree : m_Trees)
		{
			constexpr float reach = 8.0f;
			if (tree.Center.x + reach < view.Min.x || tree.Center.x - reach > view.Max.x || tree.Center.y + reach < view.Min.y || tree.Center.y - reach > view.Max.y)
				continue;
			const float time = view.Time + tree.Phase;
			const glm::vec3 heading = glm::normalize(glm::vec3(glm::inverse(tree.Place) * glm::vec4(WindHeading, 0.0f)));
			const glm::vec3 across = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), heading));
			const auto gustAt = [&](const glm::vec3& point) { return Gust(time - glm::dot(point, heading) / GustSpeed, tree.Phase); };
			std::vector<glm::mat4> motion(m_Joints.size(), glm::mat4(1.0f));
			for (size_t i = 0; i < m_Joints.size(); i++)
			{
				const Joint& joint = m_Joints[i];
				glm::mat4 local(1.0f);
				switch (joint.Kind)
				{
				case Part::Trunk:
					local = Pivot(joint.Rest, across, TrunkSway * (gustAt(joint.Rest) + 0.2f * Turbulence(time * 0.7f, 3.0f)));
					break;
				case Part::Crown:
				{
					const float gust = gustAt(Crown);
					local = Pivot(joint.Rest, across, CrownSway * (gust + 0.35f * gust * Turbulence(time * 1.1f, 5.0f))) * Pivot(joint.Rest, glm::vec3(0.0f, 1.0f, 0.0f), 0.01f * Turbulence(time * 0.5f, 7.0f));
					break;
				}
				case Part::Cluster:
				{
					const float gust = gustAt(joint.Rest);
					const float rate = 2.2f + std::fmod(joint.Seed, 1.6f);
					const float flap = ClusterFlap * (0.25f + gust) * Turbulence(time * rate, joint.Seed);
					const float twist = ClusterTwist * (0.25f + gust) * Turbulence(time * rate * 0.8f, joint.Seed + 31.0f);
					const float lean = ClusterSway * gust * (0.75f + 0.25f * Turbulence(time * 0.9f, joint.Seed + 57.0f));
					const glm::vec3 outward = glm::normalize(glm::vec3(joint.Rest.x - Crown.x, 0.0f, joint.Rest.z - Crown.z) + glm::vec3(1e-4f, 0.0f, 0.0f));
					const glm::vec3 hinge = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), outward));
					local = Pivot(joint.Rest, across, lean) * Pivot(joint.Rest, hinge, flap) * Pivot(joint.Rest, outward, twist);
					break;
				}
				default:
					break;
				}
				motion[i] = joint.Parent >= 0 ? motion[size_t(joint.Parent)] * local : local;
			}
			JointMatrices palette;
			for (size_t i = 0; i < motion.size(); i++)
				palette[i] = tree.Place * motion[i];
			const uint32_t skin = renderer.AddPalette(palette, uint32_t(motion.size()));
			for (const auto* meshes : { &m_Bark, &m_Leaves })
				for (const uint32_t mesh : *meshes)
				{
					ModelInstance instance = renderer.MakeModelInstance(mesh, glm::mat4(1.0f), skin);
					instance.Shading.w = 1.0f;
					renderer.SubmitModel(mesh, instance);
				}
		}
	}
}

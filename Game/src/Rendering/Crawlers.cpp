#include "Crawlers.h"
#include "Rendering/Characters.h"
#include "Rendering/Effects.h"
#include "Rendering/Models.h"
#include <GameShared/Content/Shardfall.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr std::array<glm::vec3, CrawlerLegs / 2> Hips = { { { 0.1425f, 0.27f, 0.296f }, { 0.1875f, 0.2625f, 0.1275f }, { 0.169f, 0.2775f, -0.09f } } };
		constexpr std::array<float, CrawlerLegs / 2> Splay = { 0.86f, 0.0f, -0.711f };
		constexpr std::array<float, CrawlerLegs / 2> Reach = { 0.391f, 0.3225f, 0.391f };
		constexpr glm::vec3 Socket{ 0.0f, 0.285f, -0.1125f };
		constexpr float CrystalSink = 0.55f;
		constexpr float FemurOverlap = 0.03f;
		constexpr float RiftLean = 0.75f;
		constexpr float SizeJitter = 0.07f;
		constexpr float BreatheRate = 2.4f;
		constexpr float BreatheLift = 0.008f;
		constexpr float BreathePitch = 0.012f;
		constexpr float CrystalSway = 0.03f;
		constexpr float SwayRate = 1.3f;
		constexpr float CrystalYaw = 0.3f;
		constexpr float DebrisGravity = 9.0f;
		constexpr float DebrisRest = 0.03f;
		constexpr float HighlightRim = 0.12f;
		constexpr float CrystalPulse = 0.05f;
		constexpr float StepTrigger = 0.16f;
		constexpr float StepLead = 0.14f;
		constexpr float StepHeight = 0.14f;
		constexpr float Stride = 0.55f;
		constexpr float CorpseSeconds = 1.35f;
		constexpr float CorpseSink = 0.12f;
		constexpr float CurlSeconds = 0.28f;
		constexpr uint32_t ShatterPieces = 12;
		constexpr float TetherFlicker = 0.12f;
		constexpr glm::vec3 BiteDanger{ 1.0f, 0.24f, 0.12f };
		constexpr float GlintPeriod = 1.8f;
		constexpr float GlintSeconds = 0.24f;
		constexpr float RiftColorSeconds = 4.0f;
		constexpr float RiftHover = 1.0f;
		constexpr float RiftScale = 1.3f;
		constexpr float RiftOrbit = 0.75f;
		constexpr float CrystalLight = 5.0f;
		constexpr float FlareLight = 10.0f;
		constexpr float CrystalReach = 1.6f;
		constexpr float OrbLight = 2.5f;
		constexpr float OrbReach = 1.3f;
		constexpr float RiftLight = 4.0f;

		uint32_t Hash(uint32_t a, uint32_t b)
		{
			uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
			h ^= h >> 15;
			h *= 0x2C1B3C6Du;
			h ^= h >> 12;
			return h;
		}

		float Unit(uint32_t h)
		{
			return float(h & 0xFFFFFFu) / 16777215.0f;
		}

		glm::vec3 Hue(float h)
		{
			const glm::vec3 k = glm::abs(glm::fract(glm::vec3(h) + glm::vec3(0.0f, 2.0f / 3.0f, 1.0f / 3.0f)) * 6.0f - 3.0f) - 1.0f;
			return glm::clamp(k, 0.0f, 1.0f);
		}

		glm::vec3 Up()
		{
			return { 0.0f, 1.0f, 0.0f };
		}

		glm::mat4 Frame(const glm::vec3& base, float facing, float scale, float pitch, float roll)
		{
			return ModelTransform(base, facing, scale) * glm::rotate(glm::mat4(1.0f), pitch, glm::vec3(1.0f, 0.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), roll, glm::vec3(0.0f, 0.0f, 1.0f));
		}

		glm::mat4 Segment(const glm::vec3& from, const glm::vec3& to, float scale)
		{
			glm::vec3 z = to - from;
			const float length = glm::length(z);
			z = length > 1e-5f ? z / length : -Up();
			glm::vec3 y = Up() - z * glm::dot(Up(), z);
			if (glm::dot(y, y) < 1e-6f)
				y = glm::vec3(1.0f, 0.0f, 0.0f) - z * z.x;
			y = glm::normalize(y);
			const glm::vec3 x = glm::cross(y, z);
			glm::mat4 m(1.0f);
			m[0] = glm::vec4(x * scale, 0.0f);
			m[1] = glm::vec4(y * scale, 0.0f);
			m[2] = glm::vec4(z * scale, 0.0f);
			m[3] = glm::vec4(from, 1.0f);
			return m;
		}

		glm::mat4 Upright(const glm::vec3& center, float angle, float scale)
		{
			const glm::vec3 x(std::cos(angle), 0.0f, std::sin(angle));
			const glm::vec3 y(std::sin(angle), 0.0f, -std::cos(angle));
			glm::mat4 m(1.0f);
			m[0] = glm::vec4(x * scale, 0.0f);
			m[1] = glm::vec4(y * scale, 0.0f);
			m[2] = glm::vec4(Up() * scale, 0.0f);
			m[3] = glm::vec4(center, 1.0f);
			return m;
		}

		glm::vec3 Knee(const glm::vec3& hip, const glm::vec3& foot, float a, float b, const glm::vec3& outward)
		{
			glm::vec3 axis = foot - hip;
			const float span = glm::length(axis);
			const glm::vec3 direction = span > 1e-5f ? axis / span : -Up();
			const float d = std::clamp(span, std::abs(a - b) + 1e-3f, a + b - 1e-3f);
			const float along = (a * a - b * b + d * d) / (2.0f * d);
			const float rise = std::sqrt(std::max(a * a - along * along, 0.0f));
			glm::vec3 bend = Up() + outward * 0.45f;
			bend -= direction * glm::dot(bend, direction);
			bend = glm::dot(bend, bend) > 1e-6f ? glm::normalize(bend) : Up();
			return hip + direction * along + bend * rise;
		}

		uint32_t Group(uint32_t leg)
		{
			return (leg / 2 + leg % 2) % 2;
		}

		glm::vec3 HipLocal(uint32_t leg)
		{
			const float side = leg % 2 == 0 ? 1.0f : -1.0f;
			const glm::vec3 hip = Hips[leg / 2];
			return { side * hip.x, hip.y, hip.z };
		}

		glm::vec3 RestLocal(uint32_t leg)
		{
			const float side = leg % 2 == 0 ? 1.0f : -1.0f;
			const glm::vec3 hip = HipLocal(leg);
			const float angle = Splay[leg / 2];
			return { hip.x + side * std::cos(angle) * Reach[leg / 2], 0.0f, hip.z + std::sin(angle) * Reach[leg / 2] };
		}

		glm::vec3 Place(const glm::mat4& frame, const glm::vec3& local)
		{
			return glm::vec3(frame * glm::vec4(local, 1.0f));
		}

		struct CrawlerLook
		{
			glm::vec3 Chitin{ 1.0f };
			glm::vec3 Highlight{ 1.0f };
			glm::vec3 Eyes{ 1.0f };
			uint32_t Buds = 0;
		};

		CrawlerLook LookOf(EntityKind kind, float time)
		{
			switch (kind)
			{
			case EntityKind::JadeCrawler:
				return { .Chitin = { 0.3f, 1.0f, 0.28f }, .Highlight = { 1.0f, 2.7f, 0.4f }, .Eyes = { 1.0f, 0.18f, 0.12f } };
			case EntityKind::SapphireCrawler:
				return { .Chitin = { 0.35f, 0.45f, 1.25f }, .Highlight = { 0.6f, 1.6f, 2.2f }, .Eyes = { 0.6f, 0.85f, 1.0f }, .Buds = 1 };
			case EntityKind::RubyCrawler:
				return { .Chitin = { 2.6f, 0.35f, 0.25f }, .Highlight = { 3.2f, 1.4f, 0.35f }, .Eyes = { 1.0f, 0.75f, 0.2f }, .Buds = 2 };
			case EntityKind::AmethystCrawler:
				return { .Chitin = { 0.95f, 0.4f, 1.6f }, .Highlight = { 2.0f, 0.7f, 2.2f }, .Eyes = { 1.0f, 0.8f, 0.3f }, .Buds = 3 };
			case EntityKind::PrismaticCrawler:
				return { .Chitin = glm::mix(glm::vec3(1.0f), Hue(time * 0.3f) * 1.5f, 0.5f), .Highlight = glm::mix(glm::vec3(1.0f), Hue(time * 0.3f + 0.33f) * 2.2f, 0.6f), .Buds = 1 };
			default:
				return { .Chitin = { 1.0f, 1.0f, 1.08f }, .Highlight = { 1.7f, 1.6f, 2.1f }, .Eyes = { 0.7f, 0.9f, 1.0f } };
			}
		}

		glm::vec3 Absorption(const glm::vec3& tone)
		{
			return -glm::log(glm::max(tone, glm::vec3(0.05f))) * 4.0f;
		}
	}

	bool IsCrawlerKind(EntityKind kind)
	{
		return kind >= EntityKind::QuartzCrawler && kind <= EntityKind::CrawlerEgg;
	}

	glm::vec3 CrawlerTone(EntityKind kind, float time)
	{
		switch (kind)
		{
		case EntityKind::QuartzCrawler:
			return { 0.85f, 0.88f, 1.0f };
		case EntityKind::JadeCrawler:
			return { 0.3f, 1.0f, 0.35f };
		case EntityKind::SapphireCrawler:
			return { 0.25f, 0.42f, 1.0f };
		case EntityKind::RubyCrawler:
			return { 1.0f, 0.2f, 0.1f };
		case EntityKind::AmethystCrawler:
		case EntityKind::CrawlerEgg:
			return { 0.7f, 0.28f, 1.0f };
		default:
			return glm::mix(Hue(time * 0.3f), glm::vec3(1.0f), 0.2f);
		}
	}

	float CrawlerScale(EntityKind kind)
	{
		const EnemyDef* def = FindEnemyDef(kind);
		if (kind == EntityKind::CrawlerEgg || !def)
			return 0.9f;
		return std::pow(def->Radius / 0.32f, 0.6f);
	}

	CrawlerSystem::CrawlerSystem(const ModelLibrary& models)
	{
		const auto find = [&](std::string_view name, uint32_t& mesh)
		{
			const std::optional<uint32_t> found = models.FindMesh(name);
			mesh = found.value_or(0);
			return found.has_value();
		};
		m_Ready = find("Crawler/Shell", m_Shell) && find("Crawler/Eyes", m_Eyes) && find("Crawler/Crystal", m_Crystal)
			&& find("Crawler/EggShell", m_EggShell) && find("Crawler/EggGem", m_EggGem) && find("Crawler/Shard", m_ShardMesh);
		for (uint32_t leg = 0; leg < CrawlerLegs; leg++)
			m_Ready = m_Ready && find("Crawler/Femur" + std::to_string(leg), m_Femurs[leg]) && find("Crawler/Tibia" + std::to_string(leg), m_Tibias[leg]);
		for (uint32_t bud = 0; bud < CrawlerBuds; bud++)
			m_Ready = m_Ready && find("Crawler/Bud" + std::to_string(bud), m_Buds[bud]);
		if (!m_Ready)
			return;
		for (uint32_t leg = 0; leg < CrawlerLegs; leg++)
		{
			m_FemurLengths[leg] = models.GetMesh(m_Femurs[leg]).Max.z - FemurOverlap;
			m_TibiaLengths[leg] = models.GetMesh(m_Tibias[leg]).Max.z;
		}
		const auto& crystal = models.GetMesh(m_Crystal);
		m_CrystalTip = { 0.0f, crystal.Max.y * 0.92f, crystal.Min.z * 0.92f };
		for (uint32_t chunk = 0;; chunk++)
		{
			const std::optional<uint32_t> mesh = models.FindMesh("Crawler/Chunk" + std::to_string(chunk));
			if (!mesh)
				break;
			const auto& data = models.GetMesh(*mesh);
			m_Chunks.push_back(*mesh);
			m_ChunkCentres.push_back((data.Min + data.Max) * 0.5f);
		}
	}

	void CrawlerSystem::Clear()
	{
		m_Gaits.clear();
		m_Corpses.clear();
		m_Shards.clear();
		m_Debris.clear();
		m_Orbs.clear();
	}

	void CrawlerSystem::Update(float dt, std::span<const EntityState> entities, const GroundSampler& ground)
	{
		std::vector<uint32_t> seen;
		for (const EntityState& entity : entities)
		{
			if (!IsCrawlerKind(entity.Kind))
				continue;
			seen.push_back(entity.NetId);
			const auto [it, inserted] = m_Gaits.try_emplace(entity.NetId);
			CrawlerGait& gait = it->second;
			if (inserted)
				gait.Seed = Unit(Hash(entity.NetId, 31u));
			gait.Scale = CrawlerScale(entity.Kind) * entity.GetScale() * (1.0f - SizeJitter + 2.0f * SizeJitter * gait.Seed);
			gait.Idle += dt;
			const float s = gait.Scale;
			const glm::vec3 target(entity.Position.x, ground(entity.Position), entity.Position.y);
			glm::vec2 moved = (glm::vec2(target.x, target.z) - glm::vec2(gait.Base.x, gait.Base.z)) / std::max(dt, 1e-4f);
			const bool jumped = !gait.Placed || glm::length(moved) * dt > 2.5f;
			if (jumped)
			{
				gait.Base = target;
				gait.Facing = entity.Facing;
				gait.Velocity = glm::vec2(0.0f);
				moved = glm::vec2(0.0f);
				const glm::mat4 frame = ModelTransform(target, entity.Facing, s);
				for (uint32_t leg = 0; leg < CrawlerLegs; leg++)
				{
					gait.Feet[leg] = Place(frame, RestLocal(leg));
					gait.Feet[leg].y = ground(Vec2(gait.Feet[leg].x, gait.Feet[leg].z));
					gait.Step[leg] = 0.0f;
				}
				gait.Sprout = entity.Has(EntityFlags::Pending) ? 0.0f : 1.0f;
				gait.Placed = true;
			}
			gait.Velocity = glm::mix(gait.Velocity, moved, 1.0f - std::exp(-10.0f * dt));
			gait.Base = target;
			const float turn = AngleBetween(gait.Facing, entity.Facing) * (1.0f - std::exp(-16.0f * dt));
			gait.Facing += turn;
			gait.Turn = glm::mix(gait.Turn, turn / std::max(dt, 1e-4f), 1.0f - std::exp(-8.0f * dt));
			const float speed = glm::length(gait.Velocity);
			gait.Cycle += speed * dt / (Stride * s);
			const bool attacking = entity.Has(EntityFlags::Attacking);
			gait.Windup = attacking || entity.Kind == EntityKind::CrawlerEgg ? gait.Windup + dt : 0.0f;
			const float crouch = attacking ? (entity.Kind == EntityKind::JadeCrawler ? 1.0f : 0.45f) : 0.0f;
			gait.Crouch = glm::mix(gait.Crouch, crouch, 1.0f - std::exp(-10.0f * dt));
			gait.Flare = glm::mix(gait.Flare, attacking ? 1.0f : 0.0f, 1.0f - std::exp((attacking ? -9.0f : -3.0f) * dt));
			gait.Lunge = entity.Has(EntityFlags::Dashing) ? 1.0f : std::max(gait.Lunge - dt * 4.0f, 0.0f);
			gait.Shield = glm::mix(gait.Shield, entity.Has(EntityFlags::Barrier) ? 1.0f : 0.0f, 1.0f - std::exp(-6.0f * dt));
			if (entity.Has(EntityFlags::Pending))
				gait.Sprout = gait.Sprout >= 1.0f ? 0.0f : std::min(gait.Sprout + dt / SproutSeconds, 0.999f);
			else
				gait.Sprout = 1.0f;
			gait.Burrow = entity.Has(EntityFlags::Falling) ? std::min(gait.Burrow + dt / BurrowWarning, 1.0f) : 0.0f;

			const glm::mat4 frame = ModelTransform(target, gait.Facing, s);
			const glm::vec3 lead = glm::vec3(gait.Velocity.x, 0.0f, gait.Velocity.y) * StepLead;
			std::array<glm::vec3, CrawlerLegs> desired{};
			bool stepping = false;
			const float duration = std::clamp(0.3f / (1.0f + speed * 0.5f), 0.08f, 0.24f);
			for (uint32_t leg = 0; leg < CrawlerLegs; leg++)
			{
				desired[leg] = Place(frame, RestLocal(leg)) + lead;
				desired[leg].y = ground(Vec2(desired[leg].x, desired[leg].z));
				if (gait.Step[leg] > 0.0f)
				{
					gait.Step[leg] += dt / duration;
					const float t = std::min(gait.Step[leg], 1.0f);
					const float eased = t * t * (3.0f - 2.0f * t);
					gait.Feet[leg] = glm::mix(gait.From[leg], desired[leg], eased) + Up() * (std::sin(t * Pi) * StepHeight * s);
					if (gait.Step[leg] >= 1.0f)
					{
						gait.Step[leg] = 0.0f;
						gait.Feet[leg] = desired[leg];
					}
					stepping = stepping || gait.Step[leg] > 0.0f;
				}
				else
					gait.Feet[leg].y = ground(Vec2(gait.Feet[leg].x, gait.Feet[leg].z));
			}
			if (!stepping)
			{
				std::array<float, 2> worst{};
				for (uint32_t leg = 0; leg < CrawlerLegs; leg++)
				{
					const glm::vec3 miss = desired[leg] - gait.Feet[leg];
					worst[Group(leg)] = std::max(worst[Group(leg)], std::sqrt(miss.x * miss.x + miss.z * miss.z));
				}
				const uint32_t group = worst[0] >= worst[1] ? 0u : 1u;
				if (worst[group] > StepTrigger * s)
					for (uint32_t leg = 0; leg < CrawlerLegs; leg++)
					{
						const glm::vec3 miss = desired[leg] - gait.Feet[leg];
						if (Group(leg) == group && miss.x * miss.x + miss.z * miss.z > 0.0009f * s * s)
						{
							gait.From[leg] = gait.Feet[leg];
							gait.Step[leg] = 1e-4f;
						}
					}
			}
		}
		std::erase_if(m_Gaits, [&](const auto& item) { return std::find(seen.begin(), seen.end(), item.first) == seen.end(); });

		for (Corpse& corpse : m_Corpses)
		{
			corpse.Age += dt;
			const float curl = std::min(corpse.Age / CurlSeconds, 1.0f);
			for (uint32_t leg = 0; leg < CrawlerLegs; leg++)
				corpse.Gait.Feet[leg] = glm::mix(corpse.Gait.Feet[leg], corpse.Tucked[leg], 1.0f - std::exp(-12.0f * dt * (0.4f + curl)));
			corpse.Gait.Crouch = std::min(corpse.Gait.Crouch + dt * 3.0f, 1.4f);
		}
		std::erase_if(m_Corpses, [](const Corpse& corpse) { return corpse.Age >= CorpseSeconds; });
		UpdateDiamondShards(m_Shards, dt);
		for (Debris& debris : m_Debris)
		{
			debris.Age += dt;
			debris.Velocity.y -= DebrisGravity * dt;
			debris.Position += debris.Velocity * dt;
			debris.Angle += debris.Spin * dt;
			if (debris.Position.y < debris.Floor + DebrisRest && debris.Velocity.y < 0.0f)
			{
				debris.Position.y = debris.Floor + DebrisRest;
				debris.Velocity *= glm::vec3(0.55f, -0.3f, 0.55f);
				debris.Spin *= 0.5f;
			}
		}
		std::erase_if(m_Debris, [](const Debris& debris) { return debris.Age >= debris.Life; });
	}

	CrawlerPose CrawlerSystem::Pose(const CrawlerGait& gait) const
	{
		CrawlerPose pose;
		const float s = gait.Scale;
		const float speed = glm::length(gait.Velocity);
		const glm::vec3 forward = Heading(gait.Facing);
		const float ahead = glm::dot(glm::vec3(gait.Velocity.x, 0.0f, gait.Velocity.y), forward);
		const float bob = 0.012f * std::sin(gait.Cycle * 4.0f * Pi) * std::min(speed / 2.0f, 1.0f);
		const float emerge = std::clamp((gait.Sprout - 0.35f) / 0.65f, 0.0f, 1.0f);
		const float sink = (1.0f - emerge) * 0.42f + gait.Burrow * gait.Burrow * 0.55f;
		const float breathe = std::sin(gait.Idle * BreatheRate + gait.Seed * 2.0f * Pi);
		const float lift = (bob + breathe * BreatheLift - gait.Crouch * 0.07f + gait.Lunge * 0.03f - sink) * s;
		const float pitch = std::clamp(ahead * 0.018f, -0.06f, 0.08f) + gait.Crouch * 0.12f - gait.Lunge * 0.08f + breathe * BreathePitch;
		const float roll = std::clamp(-gait.Turn * 0.05f, -0.12f, 0.12f);
		pose.Body = Frame(gait.Base + Up() * lift, gait.Facing, s, pitch, roll);
		const float grow = std::clamp(gait.Sprout / 0.6f, 0.0f, 1.0f);
		const glm::vec3 socket = Place(pose.Body, Socket);
		const glm::vec3 seat = glm::mix(gait.Base - Up() * (CrystalSink * s), socket, grow);
		const float crystalScale = s * (0.35f + 0.65f * grow) * (1.0f + 0.05f * gait.Flare);
		pose.Seat = Frame(seat, gait.Facing, crystalScale, pitch, roll);
		const float sway = CrystalSway * std::sin(gait.Idle * SwayRate + gait.Seed * 9.0f);
		pose.Crystal = pose.Seat * glm::rotate(glm::mat4(1.0f), (gait.Seed - 0.5f) * CrystalYaw, Up()) * glm::rotate(glm::mat4(1.0f), sway, glm::vec3(1.0f, 0.0f, 0.0f));
		pose.Tip = Place(pose.Crystal, m_CrystalTip);
		pose.Core = Place(pose.Crystal, m_CrystalTip * 0.3f);
		const glm::vec3 right = glm::vec3(pose.Body[0]) / s;
		for (uint32_t leg = 0; leg < CrawlerLegs; leg++)
		{
			const glm::vec3 hip = Place(pose.Body, HipLocal(leg));
			glm::vec3 foot = gait.Feet[leg];
			if (emerge < 1.0f)
				foot = glm::mix(hip - Up() * (0.1f * s), foot, emerge);
			const glm::vec3 outward = right * (leg % 2 == 0 ? 1.0f : -1.0f);
			const glm::vec3 knee = Knee(hip, foot, m_FemurLengths[leg] * s, m_TibiaLengths[leg] * s, outward);
			pose.Femurs[leg] = Segment(hip, knee, s);
			pose.Tibias[leg] = Segment(knee, foot, s);
		}
		return pose;
	}

	void CrawlerSystem::DrawBody(WorldRenderer& renderer, EntityKind kind, const CrawlerGait& gait, const CrawlerPose& pose, float time, const glm::vec4& flash, const ModelDissolve& dissolve, bool crystal) const
	{
		const glm::vec3 tone = CrawlerTone(kind, time);
		const CrawlerLook look = LookOf(kind, time);
		const float ground = gait.Base.y;
		const glm::vec4 tint(look.Chitin, 0.0f);
		const glm::vec4 highlight(glm::mix(look.Highlight, CrawlerTone(EntityKind::AmethystCrawler, time) * 2.0f, 0.6f * gait.Shield), HighlightRim + 0.3f * gait.Shield);
		renderer.DrawModel(m_Shell, pose.Body, NoPalette, tint, 0.0f, ground, highlight, MeshTwoTone, flash, dissolve);
		renderer.DrawModel(m_Eyes, pose.Body, NoPalette, glm::vec4(look.Eyes, 0.0f), 0.82f + 0.08f * gait.Flare, ground, glm::vec4(0.0f), 0, flash, dissolve);
		for (uint32_t leg = 0; leg < CrawlerLegs; leg++)
		{
			renderer.DrawModel(m_Femurs[leg], pose.Femurs[leg], NoPalette, tint, 0.0f, ground, highlight, MeshTwoTone, flash, dissolve);
			renderer.DrawModel(m_Tibias[leg], pose.Tibias[leg], NoPalette, tint, 0.0f, ground, highlight, MeshTwoTone, flash, dissolve);
		}
		if (!crystal)
			return;
		const float pulse = 0.5f + 0.5f * std::sin(time * 18.0f);
		const float glow = 0.1f + gait.Flare * (0.55f + 0.35f * pulse) + 0.5f * flash.a;
		renderer.DrawModel(m_Crystal, pose.Crystal, NoPalette, glm::vec4(tone, 0.0f), glow, ground, glm::vec4(tone, 0.5f + gait.Flare), 0, glm::vec4(0.0f), dissolve);
		for (uint32_t bud = 0; bud < look.Buds; bud++)
			renderer.DrawModel(m_Buds[bud], pose.Seat, NoPalette, glm::vec4(tone, 0.0f), glow, ground, glm::vec4(tone, 0.5f + gait.Flare), 0, glm::vec4(0.0f), dissolve);
	}

	void CrawlerSystem::Draw(WorldRenderer& renderer, const EntityState& entity, float time, const glm::vec4& flash) const
	{
		const auto it = m_Gaits.find(entity.NetId);
		if (!m_Ready || it == m_Gaits.end())
			return;
		const CrawlerGait& gait = it->second;
		if (entity.Kind == EntityKind::CrawlerEgg)
		{
			DrawEgg(renderer, entity, gait, time, flash);
			return;
		}
		const CrawlerPose pose = Pose(gait);
		const glm::vec3 tone = CrawlerTone(entity.Kind, time);
		const float s = gait.Scale;
		ModelDissolve dissolve;
		if (gait.Sprout < 1.0f)
			dissolve = { .Amount = 1.0f - std::clamp((gait.Sprout - 0.3f) / 0.7f, 0.0f, 1.0f), .Edge = tone * 3.0f, .Width = 0.08f, .Scale = 7.0f, .Sweep = 1.0f, .Seed = float(entity.NetId % 97) };
		if (dissolve.Amount < 0.999f)
			DrawBody(renderer, entity.Kind, gait, pose, time, flash, dissolve, false);
		const float pulse = 0.5f + 0.5f * std::sin(time * 18.0f);
		const float breath = CrystalPulse * (0.5f + 0.5f * std::sin(gait.Idle * 1.7f + gait.Seed * 5.0f));
		const float glow = 0.18f + breath + gait.Flare * (0.32f + 0.18f * pulse) + 0.4f * flash.a + (1.0f - gait.Sprout) * 0.5f;
		renderer.DrawModel(m_Crystal, pose.Crystal, NoPalette, glm::vec4(tone, 0.0f), glow, gait.Base.y, glm::vec4(tone, 0.3f + gait.Flare), 0, glm::vec4(0.0f), {});
		for (uint32_t bud = 0; bud < LookOf(entity.Kind, time).Buds; bud++)
			renderer.DrawModel(m_Buds[bud], pose.Seat, NoPalette, glm::vec4(tone, 0.0f), glow, gait.Base.y, glm::vec4(tone, 0.3f + gait.Flare), 0, glm::vec4(0.0f), {});
		const float window = std::floor(time / GlintPeriod);
		const uint32_t glint = Hash(entity.NetId, uint32_t(window));
		const float since = time - window * GlintPeriod - Unit(glint) * (GlintPeriod - GlintSeconds);
		if (since >= 0.0f && since < GlintSeconds && gait.Sprout >= 1.0f)
		{
			const glm::vec3 seat(pose.Crystal[3]);
			const glm::vec3 point = glm::mix(seat, pose.Tip, 0.35f + 0.6f * Unit(Hash(glint, 1u))) + Heading(Unit(Hash(glint, 2u)) * 2.0f * Pi) * (0.08f * s);
			DrawStar(renderer, point, 0.16f * s * std::sin(Pi * since / GlintSeconds), glm::mix(tone, glm::vec3(1.0f), 0.6f) * 1.3f, time * 2.0f);
		}
		DrawGlow(renderer, pose.Core, (0.17f + 0.12f * gait.Flare) * s, tone * (0.18f + 1.1f * gait.Flare * (0.7f + 0.3f * pulse)));
		renderer.DrawLight({ .Position = pose.Core, .Radius = CrystalReach * s * (1.0f + 0.5f * gait.Flare), .Color = tone * (gait.Sprout * (CrystalLight * (1.0f + breath * 4.0f) + FlareLight * gait.Flare * (0.7f + 0.3f * pulse)) + 4.0f * flash.a) });
		DrawGroundGlow(renderer, gait.Base, 0.7f * s, tone, 0.12f + 0.5f * gait.Flare + (1.0f - gait.Sprout) * 0.6f);
		if (gait.Sprout < 1.0f)
			DrawGroundRing(renderer, gait.Base, (0.25f + 0.45f * gait.Sprout) * s, tone, 1.2f * (1.0f - gait.Sprout), 0.04f, 6.0f, 2.0f);
		if (!entity.Has(EntityFlags::Attacking))
			return;
		const EnemyDef* def = FindEnemyDef(entity.Kind);
		if (!def)
			return;
		const float progress = std::clamp(gait.Windup / std::max(def->Windup, 0.05f), 0.0f, 1.0f);
		const float intensity = 0.75f + 0.35f * pulse;
		const AbilityDef& attack = GetAbility(def->Attack);
		switch (entity.Kind)
		{
		case EntityKind::JadeCrawler:
			DrawLaneTelegraph(renderer, gait.Base, entity.Facing, attack.Reach + def->Radius, attack.Width, BiteDanger, intensity, progress);
			break;
		case EntityKind::SapphireCrawler:
		case EntityKind::AmethystCrawler:
		{
			const uint32_t shots = 1u + def->Volley;
			for (uint32_t shot = 0; shot < shots; shot++)
			{
				const float offset = shots > 1 ? attack.ProjectileSpread * (float(shot) / float(shots - 1) - 0.5f) : 0.0f;
				DrawLaneTelegraph(renderer, gait.Base + Heading(entity.Facing + offset) * attack.SpawnOffset, entity.Facing + offset, attack.ProjectileRange * 0.55f, attack.ProjectileRadius * 2.0f, tone, intensity * 0.8f, progress);
			}
			break;
		}
		default:
			DrawSectorTelegraph(renderer, gait.Base, entity.Facing, def->Lunge + attack.Reach, attack.HalfArc, BiteDanger, intensity, progress);
			break;
		}
	}

	void CrawlerSystem::DrawEgg(WorldRenderer& renderer, const EntityState& entity, const CrawlerGait& gait, float time, const glm::vec4& flash) const
	{
		const glm::vec3 tone = CrawlerTone(entity.Kind, time);
		const float ripe = std::clamp(gait.Windup / EggHatchSeconds, 0.0f, 1.0f);
		const float wobble = ripe * ripe * 0.12f * std::sin(time * (14.0f + 20.0f * ripe) + float(entity.NetId));
		const float grow = std::clamp(gait.Sprout * 1.4f, 0.0f, 1.0f);
		const glm::mat4 frame = Frame(gait.Base, gait.Facing, grow, wobble, wobble * 0.6f);
		renderer.DrawModel(m_EggShell, frame, NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, gait.Base.y, glm::vec4(tone * 0.4f, 0.4f), 0, flash);
		const float glow = 0.15f + 0.75f * ripe * (0.6f + 0.4f * std::sin(time * (6.0f + 18.0f * ripe)));
		renderer.DrawModel(m_EggGem, frame, NoPalette, glm::vec4(tone, 0.0f), glow, gait.Base.y, glm::vec4(tone, 0.6f), 0, glm::vec4(0.0f));
		DrawGlow(renderer, gait.Base + Up() * 0.3f, 0.1f + 0.08f * ripe, tone * (0.25f + 0.9f * ripe));
		DrawGroundGlow(renderer, gait.Base, 0.4f, tone, 0.15f + 0.4f * ripe);
	}

	void CrawlerSystem::DrawTethers(WorldRenderer& renderer, std::span<const EntityState> entities, float time) const
	{
		const glm::vec3 brood = CrawlerTone(EntityKind::AmethystCrawler, time);
		for (const EntityState& entity : entities)
		{
			if (!entity.Has(EntityFlags::Barrier) || !IsCrawlerKind(entity.Kind))
				continue;
			const EntityState* mother = nullptr;
			float best = (BroodReach + 0.6f) * (BroodReach + 0.6f);
			for (const EntityState& other : entities)
				if (other.Kind == EntityKind::AmethystCrawler && !other.Has(EntityFlags::Pending))
					if (const float distance = LengthSquared(other.Position - entity.Position); distance < best)
					{
						best = distance;
						mother = &other;
					}
			const auto from = mother ? m_Gaits.find(mother->NetId) : m_Gaits.end();
			const auto to = m_Gaits.find(entity.NetId);
			if (from == m_Gaits.end() || to == m_Gaits.end())
				continue;
			const CrawlerPose source = Pose(from->second);
			const CrawlerPose target = Pose(to->second);
			const float strength = to->second.Shield;
			LightningShape thread;
			thread.From = source.Core;
			thread.To = target.Core;
			thread.Seed = Hash(entity.NetId, uint32_t(time / TetherFlicker));
			thread.Core = 0.007f;
			thread.Glow = 0.07f;
			thread.Jag = 0.06f;
			thread.Branches = 0.0f;
			thread.Depth = 1;
			thread.Intensity = strength * (0.32f + 0.12f * std::sin(time * 9.0f + float(entity.NetId)));
			thread.CoreColor = brood * 1.6f + glm::vec3(0.25f);
			thread.GlowColor = brood;
			renderer.DrawLightning(thread);
		}
	}

	void CrawlerSystem::Shatter(EntityKind kind, const CrawlerPose& pose, float scale, float floor, uint32_t seed)
	{
		const glm::vec3 base = glm::vec3(pose.Crystal[3]);
		glm::mat4 basis = pose.Crystal;
		basis[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		for (uint32_t chunk = 0; chunk < m_Chunks.size(); chunk++)
		{
			const uint32_t h = Hash(seed, 100u + chunk);
			Debris debris;
			debris.Chunk = chunk;
			debris.Basis = basis;
			debris.Position = Place(pose.Crystal, m_ChunkCentres[chunk]);
			glm::vec3 out = debris.Position - pose.Core;
			out.y = 0.0f;
			out = glm::dot(out, out) > 1e-6f ? glm::normalize(out) : Heading(Unit(h) * 2.0f * Pi);
			debris.Velocity = (out * (1.0f + 1.4f * Unit(Hash(h, 1u))) + Up() * (1.6f + 1.6f * Unit(Hash(h, 2u)))) * std::sqrt(scale);
			debris.Axis = glm::normalize(glm::vec3(Unit(Hash(h, 3u)) - 0.5f, Unit(Hash(h, 4u)) - 0.5f, Unit(Hash(h, 5u)) - 0.5f) + glm::vec3(0.0f, 0.01f, 0.0f));
			debris.Spin = (Unit(Hash(h, 6u)) - 0.5f) * 14.0f;
			debris.Life = 1.0f + 0.6f * Unit(Hash(h, 7u));
			debris.Floor = floor;
			debris.Tone = CrawlerTone(kind, 0.0f);
			m_Debris.push_back(debris);
		}
		const float pieces = float(ShatterPieces);
		for (uint32_t i = 0; i < uint32_t(pieces); i++)
		{
			const uint32_t h = Hash(seed, i);
			const float angle = Unit(h) * 2.0f * Pi;
			const float up = Unit(Hash(h, 1u));
			const glm::vec3 out(std::cos(angle), 0.0f, std::sin(angle));
			DiamondShard shard;
			shard.Center = glm::mix(base, pose.Tip, up) + out * (0.05f * scale);
			const float size = (0.05f + 0.08f * Unit(Hash(h, 2u))) * scale;
			for (uint32_t c = 0; c < 3; c++)
			{
				const float a = angle + float(c) * 2.1f + Unit(Hash(h, 3u + c)) * 0.8f;
				shard.Corners[c] = glm::vec3(std::cos(a) * size, (Unit(Hash(h, 6u + c)) - 0.5f) * size * 2.4f, std::sin(a) * size);
			}
			shard.Velocity = out * (0.9f + 1.3f * Unit(Hash(h, 9u))) + Up() * (1.5f + 1.8f * up);
			shard.Axis = glm::normalize(glm::vec3(Unit(Hash(h, 10u)) - 0.5f, 0.6f, Unit(Hash(h, 11u)) - 0.5f));
			shard.Spin = (Unit(Hash(h, 12u)) - 0.5f) * 22.0f;
			shard.Floor = floor;
			shard.Life = 0.7f + 0.5f * Unit(Hash(h, 13u));
			shard.Seed = h | uint32_t(kind) << 24;
			m_Shards.push_back(shard);
		}
	}

	void CrawlerSystem::Vanish(const EntityState& last, bool killed, EffectSystem& effects)
	{
		const auto it = m_Gaits.find(last.NetId);
		if (!m_Ready || it == m_Gaits.end())
			return;
		const CrawlerGait& gait = it->second;
		const glm::vec3 tone = CrawlerTone(last.Kind, 0.0f);
		const float s = gait.Scale;
		if (last.Kind == EntityKind::CrawlerEgg)
		{
			effects.Burst(Fx::CrystalBurst, gait.Base + Up() * 0.2f, Up(), killed ? 0.8f : 0.6f, tone);
			effects.Burst(Fx::Debris, gait.Base + Up() * 0.1f, Up(), 0.5f, tone);
			return;
		}
		if (!killed)
		{
			if (last.Has(EntityFlags::Falling))
				effects.Burst(Fx::Crumble, gait.Base + Up() * 0.1f, Up(), 1.2f * s, tone);
			return;
		}
		const CrawlerPose pose = Pose(gait);
		Corpse corpse{ .Kind = last.Kind, .Gait = gait, .Seed = Hash(last.NetId, 77u) };
		corpse.Gait.Flare = 0.0f;
		corpse.Gait.Lunge = 0.0f;
		for (uint32_t leg = 0; leg < CrawlerLegs; leg++)
		{
			const glm::vec3 hip = Place(pose.Body, HipLocal(leg));
			corpse.Tucked[leg] = glm::mix(gait.Feet[leg], glm::vec3(hip.x, gait.Base.y + 0.06f * s, hip.z), 0.55f);
		}
		m_Corpses.push_back(corpse);
		Shatter(last.Kind, pose, s, gait.Base.y, corpse.Seed);
		effects.Burst(Fx::CrystalBurst, pose.Core, Up(), 0.75f * s, tone * 0.5f);
		effects.Burst(Fx::SpearShatter, pose.Core + Up() * (0.15f * s), Up(), 0.4f * s, tone * 0.8f);
	}

	void CrawlerSystem::DrawRemains(WorldRenderer& renderer, float time) const
	{
		if (!m_Ready)
			return;
		for (const Corpse& corpse : m_Corpses)
		{
			const glm::vec3 tone = CrawlerTone(corpse.Kind, time);
			const float fade = std::clamp((corpse.Age - 0.22f) / (CorpseSeconds - 0.3f), 0.0f, 1.0f);
			const ModelDissolve dissolve{ .Amount = std::max(fade, 0.001f), .Edge = tone * 0.75f, .Width = 0.03f, .Scale = 3.2f, .Sweep = -1.0f, .Seed = float(corpse.Seed % 101) };
			CrawlerGait gait = corpse.Gait;
			const float sink = CorpseSink * gait.Scale * fade * fade;
			gait.Base.y -= sink;
			for (glm::vec3& foot : gait.Feet)
				foot.y -= sink;
			DrawBody(renderer, corpse.Kind, gait, Pose(gait), time, glm::vec4(0.0f), dissolve, false);
		}
		for (const Debris& debris : m_Debris)
		{
			const float t = debris.Age / debris.Life;
			const float shrink = 1.0f - glm::smoothstep(0.55f, 1.0f, t);
			const glm::mat4 transform = glm::translate(glm::mat4(1.0f), debris.Position) * glm::rotate(glm::mat4(1.0f), debris.Angle, debris.Axis) * glm::scale(glm::mat4(1.0f), glm::vec3(shrink)) * debris.Basis * glm::translate(glm::mat4(1.0f), -m_ChunkCentres[debris.Chunk]);
			renderer.DrawModel(m_Chunks[debris.Chunk], transform, NoPalette, glm::vec4(debris.Tone, 0.0f), 0.15f + 0.6f * std::exp(-debris.Age * 6.0f), debris.Floor, glm::vec4(debris.Tone, 0.4f));
		}
		for (const DiamondShard& shard : m_Shards)
		{
			const glm::vec3 tone = CrawlerTone(EntityKind(shard.Seed >> 24), time);
			const float t = std::clamp(shard.Age / shard.Life, 0.0f, 1.0f);
			const float scale = 1.0f - 0.5f * t;
			const auto turned = [&](const glm::vec3& corner)
			{
				const float c = std::cos(shard.Angle);
				const float s = std::sin(shard.Angle);
				return corner * c + glm::cross(shard.Axis, corner) * s + shard.Axis * glm::dot(shard.Axis, corner) * (1.0f - c);
			};
			GlassFacet glass;
			glass.A = shard.Center + turned(shard.Corners[0]) * scale;
			glass.B = shard.Center + turned(shard.Corners[1]) * scale;
			glass.C = shard.Center + turned(shard.Corners[2]) * scale;
			glass.Absorption = Absorption(tone);
			glass.Thickness = 0.06f;
			glass.Wedge = 0.4f;
			glass.Edge = tone * 1.6f;
			glass.Ior = 1.62f;
			glass.Dispersion = 0.25f;
			glass.Floor = shard.Floor;
			glass.Flash = std::exp(-shard.Age * 7.0f);
			glass.Fade = 1.0f - std::clamp((t - 0.45f) / 0.55f, 0.0f, 1.0f);
			glass.Seed = shard.Seed;
			renderer.DrawFacet(glass);
		}
	}

	std::optional<glm::vec3> CrawlerSystem::CrystalTip(uint32_t netId) const
	{
		const auto it = m_Gaits.find(netId);
		if (it == m_Gaits.end())
			return std::nullopt;
		return Pose(it->second).Tip;
	}

	void CrawlerSystem::DrawShard(WorldRenderer& renderer, const glm::vec3& position, float facing, const glm::vec3& tone, float time) const
	{
		if (!m_Ready)
			return;
		renderer.DrawModel(m_ShardMesh, ModelTransform(position, facing, 1.5f) * glm::rotate(glm::mat4(1.0f), time * 9.0f, glm::vec3(0.0f, 0.0f, 1.0f)), NoPalette, glm::vec4(tone, 0.0f), 0.6f, position.y - 0.6f, glm::vec4(tone, 0.8f));
		DrawGlow(renderer, position, 0.22f, tone * 1.4f);
	}

	void CrawlerSystem::DrawRift(WorldRenderer& renderer, const glm::vec3& base, float time, bool open) const
	{
		if (!m_Ready)
			return;
		constexpr uint32_t Tiers = 5;
		const float cycle = time / RiftColorSeconds;
		const float blend = glm::smoothstep(0.7f, 1.0f, glm::fract(cycle));
		const uint32_t from = uint32_t(cycle) % Tiers;
		const glm::vec3 tone = glm::mix(CrawlerTone(RunOnlyKinds[from], time), CrawlerTone(RunOnlyKinds[(from + 1) % Tiers], time), blend);
		const float pulse = 0.5f + 0.5f * std::sin(time * 2.2f);
		const glm::vec3 seat = base + Up() * (RiftHover + 0.08f * std::sin(time * 1.3f));
		renderer.DrawModel(m_Crystal, ModelTransform(seat, time * 0.35f, RiftScale) * glm::rotate(glm::mat4(1.0f), RiftLean, glm::vec3(1.0f, 0.0f, 0.0f)), NoPalette, glm::vec4(tone, 0.0f), 0.26f + 0.18f * pulse + (open ? 0.2f : 0.0f), base.y, glm::vec4(tone, 0.35f));
		DrawGlow(renderer, seat + Up() * (0.35f * RiftScale), (0.38f + (open ? 0.1f : 0.0f)) * RiftScale, tone * (0.3f + 0.15f * pulse));
		renderer.DrawLight({ .Position = seat + Up() * (0.35f * RiftScale), .Radius = 4.5f, .Color = tone * (RiftLight * (1.0f + 0.3f * pulse + (open ? 0.4f : 0.0f))) });
		DrawGroundGlow(renderer, base, 1.7f, tone, 0.3f + 0.15f * pulse + (open ? 0.25f : 0.0f));
		DrawGroundRing(renderer, base, 1.25f + 0.05f * std::sin(time * 2.0f), tone, 0.55f + (open ? 0.6f : 0.0f), 0.035f, 9.0f, 0.4f);
		for (uint32_t i = 0; i < 3; i++)
		{
			const float angle = time * 0.9f + float(i) * 2.0f * Pi / 3.0f;
			const glm::vec3 point = seat + Up() * (0.45f * RiftScale + 0.12f * std::sin(time * 1.7f + float(i))) + Heading(angle) * RiftOrbit;
			renderer.DrawModel(m_ShardMesh, Upright(point, angle, 1.6f), NoPalette, glm::vec4(tone, 0.0f), 0.5f, base.y, glm::vec4(tone, 0.6f));
		}
	}

	void CrawlerSystem::UpdateOrbs(float dt, std::span<const Orb> orbs, std::span<const EntityState> entities, EffectSystem& effects, const GroundSampler& ground)
	{
		for (auto& [id, view] : m_Orbs)
			view.Seen = false;
		for (const Orb& orb : orbs)
		{
			const auto [it, inserted] = m_Orbs.try_emplace(orb.Id);
			OrbView& view = it->second;
			view.Target = glm::vec3(orb.Position.x, ground(orb.Position) + 0.32f, orb.Position.y);
			if (inserted)
				view.Shown = view.Target;
			view.Shown = glm::mix(view.Shown, view.Target, 1.0f - std::exp(-20.0f * dt));
			view.Value = orb.Value;
			view.Age += dt;
			view.Seen = true;
		}
		for (auto it = m_Orbs.begin(); it != m_Orbs.end();)
		{
			if (it->second.Seen)
			{
				++it;
				continue;
			}
			const OrbView& view = it->second;
			const bool taken = std::any_of(entities.begin(), entities.end(), [&](const EntityState& entity) { return entity.Kind == EntityKind::Player && LengthSquared(entity.Position - Vec2(view.Shown.x, view.Shown.z)) < 1.4f; });
			if (taken)
				effects.Burst(Fx::LootSparkle, view.Shown, Up(), 0.6f, view.Value >= 25 ? glm::vec3(1.0f, 0.8f, 0.35f) : view.Value >= 5 ? glm::vec3(0.5f, 1.0f, 0.6f) : glm::vec3(0.5f, 0.85f, 1.0f));
			it = m_Orbs.erase(it);
		}
	}

	void CrawlerSystem::DrawOrbs(WorldRenderer& renderer, float time) const
	{
		if (!m_Ready)
			return;
		for (const auto& [id, view] : m_Orbs)
		{
			const glm::vec3 color = view.Value >= 25 ? glm::vec3(1.0f, 0.78f, 0.3f) : view.Value >= 5 ? glm::vec3(0.45f, 1.0f, 0.55f) : glm::vec3(0.45f, 0.82f, 1.0f);
			const float size = 0.55f + 0.32f * std::log2(float(std::max<uint16_t>(view.Value, 1))) / 2.0f;
			const float bob = 0.05f * std::sin(time * 3.0f + float(id) * 0.7f);
			const glm::vec3 center = view.Shown + Up() * bob;
			const float appear = std::min(view.Age * 4.0f, 1.0f);
			renderer.DrawModel(m_ShardMesh, Upright(center - Up() * (0.08f * size), time * 2.2f + float(id), size * appear), NoPalette, glm::vec4(color, 0.0f), 0.55f, center.y - 0.32f, glm::vec4(color, 0.9f));
			DrawGlow(renderer, center, 0.16f * size * appear, color * 1.3f);
			renderer.DrawLight({ .Position = center, .Radius = OrbReach * size, .Color = color * (OrbLight * appear) });
			DrawGroundGlow(renderer, glm::vec3(center.x, center.y - 0.32f - bob, center.z), 0.22f * size, color, 0.25f);
		}
	}
}

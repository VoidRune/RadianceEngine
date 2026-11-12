#include "Characters.h"
#include "Rendering/CharacterAnimator.h"
#include "Rendering/ModelLibrary.h"
#include "Rendering/WorldRenderer.h"
#include <RadianceEngine/Core/Log.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

namespace Game
{
	namespace
	{
		constexpr const char* AnimNames[] = {
			"Idle_A", "Skeletons_Idle", "Melee_2H_Idle", "Ranged_Bow_Idle",
			"Hero_Idle_Look", "Hero_Idle_LookRight", "Hero_Idle_Scan", "Hero_Idle_Wand", "Hero_Idle_Stretch", "Hero_Idle_Rub", "Hero_Idle_Nod",
			"Walking_A", "Hero_Jog",
			"Running_A", "Running_HoldingBow", "Running_HoldingRifle", "Skeletons_Walking",
			"Walking_Backwards", "Running_Strafe_Left", "Running_Strafe_Right", "Hero_Strafe_Walk_Left", "Hero_Strafe_Walk_Right",
			"Dodge_Forward", "Dodge_Backward", "Dodge_Left", "Dodge_Right",
			"Hit_A", "Hit_B", "Death_A", "Skeletons_Death",
			"Melee_1H_Attack_Slice_Horizontal", "Melee_1H_Attack_Chop", "Melee_1H_Attack_Stab", "Melee_2H_Attack_Stab", "Melee_2H_Attack_Chop", "Melee_2H_Attack_Spinning", "Melee_Unarmed_Attack_Punch_A",
			"Ranged_Magic_Shoot", "Ranged_Magic_Spellcasting", "Ranged_Magic_Raise",
			"Hero_Cast_Flick", "Hero_Cast_Slash", "Hero_Cast_Backhand", "Hero_Cast_Lunge", "Hero_Cast_Hurl", "Hero_Cast_Surge", "Hero_Cast_Lash", "Hero_Cast_Grasp",
			"Hero_Cast_Lob", "Hero_Cast_Smite", "Hero_Cast_Summon", "Hero_Cast_Call", "Hero_Cast_Push", "Hero_Cast_Ward", "Hero_Cast_Nova", "Hero_Cast_Blink",
			"Ranged_Bow_Draw", "Ranged_Bow_Release", "Ranged_Bow_Draw_Up", "Ranged_Bow_Release_Up",
			"Ranged_2H_Aiming", "Ranged_2H_Shoot", "Use_Item",
		};
		static_assert(std::size(AnimNames) == size_t(Anim::Count));

		Anim FallbackOf(Anim anim)
		{
			switch (anim)
			{
			case Anim::Walk:
			case Anim::Jog:
				return Anim::Run;
			case Anim::StrafeLeftWalk:
				return Anim::StrafeLeft;
			case Anim::StrafeRightWalk:
				return Anim::StrafeRight;
			case Anim::CastFlick:
			case Anim::CastSlash:
			case Anim::CastBackhand:
			case Anim::CastLunge:
			case Anim::CastHurl:
			case Anim::CastSurge:
			case Anim::CastLash:
			case Anim::CastGrasp:
			case Anim::CastBlink:
				return Anim::MagicShoot;
			case Anim::CastSmite:
			case Anim::CastSummon:
			case Anim::CastCall:
				return Anim::MagicRaise;
			case Anim::CastLob:
			case Anim::CastPush:
			case Anim::CastWard:
			case Anim::CastNova:
				return Anim::MagicCast;
			default:
				return Anim::Count;
			}
		}

		struct OutfitModel
		{
			const char* Skeleton;
			const char* Folder;
		};

		constexpr OutfitModel OutfitModels[] = {
			{ "Hero", "Hero/" },
			{ "FrostMage", "FrostMage/" },
			{ "VoidMage", "VoidMage/" },
		};
		static_assert(std::size(OutfitModels) == size_t(Outfit::Count));

		constexpr float PlayerScale = 0.78f;
		constexpr float HeroScale = 0.9f;
		constexpr float HoodShade = 0.55f;
		constexpr float LegRadius = 0.11f;
		constexpr float ShinRadius = 0.085f;
		constexpr float TorsoRadius = 0.2f;
		constexpr float CoreRadius = 0.24f;
		constexpr float UpperArmRadius = 0.07f;
		constexpr float WandRadius = 0.025f;
		constexpr glm::vec3 WandBack{ 0.0f, -0.1f, 0.0f };
		constexpr glm::vec3 WandFront{ 0.0f, 0.42f, 0.0f };
		constexpr float ForearmRadius = 0.06f;
		constexpr float HandTip = 0.07f;
		constexpr float ColliderMargin = 0.9f;
		constexpr float LooseTail = 0.15f;

		struct SpringKind
		{
			const char* Prefix;
			float Stiffness;
			float Damping;
			float Drag;
			float Gravity;
			float Radius;
			float Bulk;
			float Margin;
		};

		constexpr SpringKind SpringKinds[] = {
			{ "cape", 25.0f, 8.0f, 0.5f, 1.0f, 0.03f, 2.3f, 0.98f },
			{ "cloak", 25.0f, 8.0f, 0.5f, 1.0f, 0.03f, 2.3f, 0.98f },
			{ "robe", 90.0f, 11.0f, 0.18f, 1.0f, 0.03f, 1.0f, ColliderMargin },
			{ "skirt", 90.0f, 11.0f, 0.18f, 1.0f, 0.03f, 1.0f, ColliderMargin },
			{ "sleeve", 160.0f, 12.0f, 0.15f, 1.0f, 0.02f, 1.0f, ColliderMargin },
			{ "hair", 400.0f, 14.0f, 0.1f, 0.6f, 0.02f, 1.0f, ColliderMargin },
			{ "strap", 220.0f, 12.0f, 0.1f, 1.0f, 0.02f, 1.0f, ColliderMargin },
		};

		const SpringKind* SpringOf(const std::string& name)
		{
			for (const SpringKind& kind : SpringKinds)
			{
				const size_t length = std::char_traits<char>::length(kind.Prefix);
				if (name.compare(0, length, kind.Prefix) == 0 && (name.size() == length || name[length] == '.'))
					return &kind;
			}
			return nullptr;
		}

		uint64_t LookKey(const PlayerLook& look)
		{
			return uint64_t(look.Weapon) | uint64_t(look.Head) << 8 | uint64_t(look.Body) << 16 | uint64_t(look.Feet) << 24 | uint64_t(PackCosmetics(look.Style)) << 32;
		}

		glm::mat4 GripOf(ItemBase base)
		{
			switch (base)
			{
			case ItemBase::Axe:
			case ItemBase::Dagger:
				return glm::rotate(glm::mat4(1.0f), Pi, glm::vec3(0.0f, 1.0f, 0.0f));
			case ItemBase::Bow:
				return glm::rotate(glm::mat4(1.0f), Pi, glm::vec3(0.0f, 0.0f, 1.0f));
			case ItemBase::Crossbow:
				return glm::rotate(glm::mat4(1.0f), -0.5f * Pi, glm::vec3(0.0f, 1.0f, 0.0f));
			default:
				return glm::mat4(1.0f);
			}
		}

		const char* OutfitOf(ItemBase base)
		{
			switch (base)
			{
			case ItemBase::Robe:
			case ItemBase::Sandals:
				return "Mage";
			case ItemBase::Tunic:
			case ItemBase::Boots:
				return "Ranger";
			case ItemBase::Plate:
			case ItemBase::Greaves:
				return "Knight";
			default:
				return "Barbarian";
			}
		}
	}

	CharacterLibrary::CharacterLibrary(const ModelLibrary& models)
		: m_Models(models)
	{
		m_Shared = models.GetSharedSkeleton();
		m_Rigs.resize(models.GetSkeletonCount());
		for (uint32_t index = 0; index < m_Rigs.size(); index++)
		{
			CharacterRig& rig = m_Rigs[index];
			for (size_t i = 0; i < size_t(Anim::Count); i++)
				rig.Clips[i] = models.FindClip(AnimNames[i], index);
			for (size_t i = 0; i < size_t(Anim::Count); i++)
			{
				const Anim fallback = FallbackOf(Anim(i));
				if (!rig.Clips[i] && fallback != Anim::Count)
					rig.Clips[i] = rig.Clips[size_t(fallback)];
				const bool optional = std::find(IdleFidgets.begin(), IdleFidgets.end(), Anim(i)) != IdleFidgets.end();
				if (!rig.Clips[i] && index == m_Shared && !optional)
					RDN_LOG_WARNING("Animation {} is missing from the model library", AnimNames[i]);
			}
			rig.Hips = models.FindJoint("hips", index);
			rig.Spine = models.FindJoint("spine", index);
			rig.Chest = models.FindJoint("chest", index);
			rig.Neck = models.FindJoint("neck", index);
			const ModelSkeleton& skeleton = models.GetSkeleton(index);
			for (uint32_t joint = 0; joint < std::min(skeleton.JointCount(), MaxModelJoints); joint++)
			{
				const std::string& name = skeleton.Names[joint];
				const bool arm = name.find("arm") != std::string::npos || name.find("wrist") != std::string::npos || name.find("hand") != std::string::npos || name.find("shoulder") != std::string::npos;
				rig.UpperBody[joint] = name == "spine" ? 0.5f : name == "chest" || name == "neck" || name == "head" || arm ? 1.0f : 0.0f;
			}
			rig.RightHand = models.FindJoint("handslot.r", index).value_or(0);
			const std::optional<uint32_t> arm[] = { models.FindJoint("upperarm.r", index), models.FindJoint("lowerarm.r", index), models.FindJoint("wrist.r", index) };
			if (arm[0] && arm[1] && arm[2])
			{
				rig.RightArm = { *arm[0], *arm[1], *arm[2] };
				rig.HasRightArm = true;
			}
			rig.LeftHand = models.FindJoint("handslot.l", index).value_or(0);
			rig.Head = models.FindJoint("head", index).value_or(0);
			Pose rest;
			RestPose(skeleton, rest);
			JointMatrices globals;
			ComputeGlobals(skeleton, rest, globals);
			rig.HipHeight = rig.Hips ? std::max(globals[*rig.Hips][3].y, 0.1f) : 1.0f;
			rig.HasLegs = rig.Hips.has_value();
			for (uint32_t side = 0; side < 2; side++)
			{
				const std::string suffix = side == 0 ? ".l" : ".r";
				const std::optional<uint32_t> chain[] = { models.FindJoint("upperleg" + suffix, index), models.FindJoint("lowerleg" + suffix, index), models.FindJoint("foot" + suffix, index) };
				if (chain[0] && chain[1] && chain[2])
				{
					rig.Legs[side] = { *chain[0], *chain[1], *chain[2], glm::inverse(glm::mat3(globals[*chain[0]])) * glm::vec3(-1.0f, 0.0f, 0.0f) };
					rig.AnkleHeight += 0.5f * globals[*chain[2]][3].y;
				}
				else
					rig.HasLegs = false;
			}
			for (uint32_t joint = 0; joint < std::min(skeleton.JointCount(), MaxModelJoints) && rig.Springs.size() < MaxSpringJoints; joint++)
			{
				const SpringKind* kind = SpringOf(skeleton.Names[joint]);
				const int32_t parent = skeleton.Parents[joint];
				if (!kind || parent < 0)
					continue;
				const bool chained = SpringOf(skeleton.Names[size_t(parent)]) != nullptr;
				glm::vec3 tail(0.0f, chained ? glm::length(skeleton.Rest[joint].Translation) : LooseTail * rig.HipHeight, 0.0f);
				for (uint32_t child = joint + 1; child < skeleton.JointCount(); child++)
					if (skeleton.Parents[child] == int32_t(joint) && SpringOf(skeleton.Names[child]))
					{
						tail = skeleton.Rest[child].Translation;
						break;
					}
				int32_t link = -1;
				for (size_t other = 0; other < rig.Springs.size(); other++)
					if (rig.Springs[other].Joint == uint32_t(parent))
						link = int32_t(other);
				rig.Springs.push_back({ .Joint = joint, .Parent = link, .Tail = tail, .Stiffness = kind->Stiffness, .Damping = kind->Damping, .Drag = kind->Drag, .Gravity = kind->Gravity, .Radius = kind->Radius * rig.HipHeight, .Bulk = kind->Bulk, .Margin = kind->Margin });
			}
			if (!rig.Springs.empty())
			{
				if (rig.HasLegs)
					for (const LegJoints& leg : rig.Legs)
					{
						rig.Colliders.push_back({ leg.Upper, leg.Lower, LegRadius * rig.HipHeight, true });
						rig.Colliders.push_back({ leg.Lower, leg.Foot, ShinRadius * rig.HipHeight, true });
					}
				if (rig.Hips && rig.Chest)
					rig.Colliders.push_back({ *rig.Hips, *rig.Chest, TorsoRadius * rig.HipHeight, true });
				if (const std::optional<uint32_t> root = models.FindJoint("root", index); root && rig.Hips)
					rig.Colliders.push_back({ *root, *rig.Hips, CoreRadius * rig.HipHeight, true });
				for (const char* side : { ".l", ".r" })
				{
					const std::optional<uint32_t> shoulder = models.FindJoint(std::string("upperarm") + side, index);
					const std::optional<uint32_t> elbow = models.FindJoint(std::string("lowerarm") + side, index);
					const std::optional<uint32_t> wrist = models.FindJoint(std::string("wrist") + side, index);
					if (shoulder && elbow)
						rig.Colliders.push_back({ *shoulder, *elbow, UpperArmRadius * rig.HipHeight });
					if (elbow && wrist)
						rig.Colliders.push_back({ *elbow, *wrist, ForearmRadius * rig.HipHeight });
				}
				if (models.FindJoint("handslot.r", index))
					rig.Colliders.push_back({ .From = rig.RightHand, .To = rig.RightHand, .Radius = WandRadius * rig.HipHeight, .FromLocal = WandBack, .ToLocal = WandFront });
				for (size_t body = 0; body < std::min<size_t>(rig.Colliders.size(), MaxBodyCapsules); body++)
				{
					const BodyCapsule& capsule = rig.Colliders[body];
					const glm::vec3 from(globals[capsule.From] * glm::vec4(capsule.FromLocal, 1.0f));
					const glm::vec3 along = glm::vec3(globals[capsule.To] * glm::vec4(capsule.ToLocal, 1.0f)) - from;
					for (SpringJoint& spring : rig.Springs)
					{
						const glm::vec3 tail(globals[spring.Joint] * glm::vec4(spring.Tail, 1.0f));
						const float span = glm::dot(along, along);
						const float t = span > 1e-8f ? std::clamp(glm::dot(tail - from, along) / span, 0.0f, 1.0f) : 0.0f;
						spring.Clear[body] = std::max(std::min(capsule.Radius * (capsule.Bulky ? spring.Bulk : 1.0f), spring.Margin * glm::length(tail - (from + along * t)) - spring.Radius), 0.0f);
					}
				}
			}
		}
		for (size_t outfit = 0; outfit < m_Outfits.size(); outfit++)
		{
			const std::optional<uint32_t> skeleton = models.FindSkeleton(OutfitModels[outfit].Skeleton);
			if (skeleton && models.FindMesh(std::string(OutfitModels[outfit].Folder) + "Hero_Head"))
				m_Outfits[outfit] = skeleton;
		}
		m_Hero = m_Outfits[size_t(Outfit::Wanderer)];

		const uint32_t rightHand = m_Rigs[m_Shared].RightHand;
		const uint32_t leftHand = m_Rigs[m_Shared].LeftHand;
		const auto enemy = [&](EntityKind kind, const char* prefix, float scale, Stance stance)
		{
			CharacterLook& look = m_Enemies[kind];
			AddPrefix(look, prefix);
			look.Skeleton = m_Shared;
			look.Scale = scale;
			look.Pose = stance;
			look.Height = TopOf(look) * scale;
			look.Undead = true;
			return std::ref(look);
		};
		enemy(EntityKind::Chaser, "Skeleton_Minion_", 0.72f, Stance::Skeleton);
		Attach(enemy(EntityKind::Shooter, "Skeleton_Rogue_", 0.8f, Stance::Shambling), "Skeleton_Crossbow", rightHand, GripOf(ItemBase::Crossbow));
		Attach(enemy(EntityKind::Brute, "Skeleton_Warrior_", 1.15f, Stance::Shambling), "Skeleton_Axe", rightHand);
		Attach(enemy(EntityKind::Caster, "Skeleton_Mage_", 0.85f, Stance::Shambling), "Skeleton_Staff", rightHand);
		CharacterLook& archivist = enemy(EntityKind::Archivist, "Skeleton_Mage_", 1.7f, Stance::Shambling);
		Attach(archivist, "Skeleton_Staff", rightHand);
		Attach(archivist, "spellbook_open", leftHand);
	}

	float CharacterLibrary::TopOf(const CharacterLook& look) const
	{
		float top = 0.0f;
		for (const CharacterPart& part : look.Parts)
			top = std::max(top, m_Models.GetMesh(part.Mesh).Max.y);
		return top;
	}

	std::optional<uint32_t> CharacterLibrary::Mesh(const char* name) const
	{
		return m_Models.FindMesh(name);
	}

	void CharacterLibrary::AddParts(CharacterLook& look, std::initializer_list<const char*> names) const
	{
		for (const char* name : names)
			if (const std::optional<uint32_t> mesh = Mesh(name))
				look.Parts.push_back({ *mesh });
	}

	void CharacterLibrary::AddPrefix(CharacterLook& look, const char* prefix) const
	{
		const std::vector<ModelMesh>& meshes = m_Models.GetData().Meshes;
		for (uint32_t i = 0; i < meshes.size(); i++)
			if (meshes[i].Skinned && meshes[i].Skeleton == m_Shared && meshes[i].Name.rfind(prefix, 0) == 0)
				look.Parts.push_back({ i });
	}

	std::vector<uint32_t> CharacterLibrary::NodeMeshes(const std::string& name) const
	{
		std::vector<uint32_t> meshes;
		std::optional<uint32_t> mesh = m_Models.FindMesh(name);
		for (uint32_t part = 1; mesh; part++)
		{
			meshes.push_back(*mesh);
			mesh = m_Models.FindMesh(name + "." + std::to_string(part));
		}
		return meshes;
	}

	void CharacterLibrary::AddNode(CharacterLook& look, const std::string& name, float shade, bool dyed) const
	{
		for (const uint32_t mesh : NodeMeshes(name))
			look.Parts.push_back({ mesh, shade, AccentOf(mesh, dyed) });
	}

	uint32_t CharacterLibrary::AccentOf(uint32_t mesh, bool dyed) const
	{
		const ModelLibraryData& data = m_Models.GetData();
		const uint32_t texture = data.Meshes[mesh].Texture;
		if (texture >= data.Textures.size())
			return 0;
		const std::string& name = data.Textures[texture].Name;
		if (name.find("Lining") != std::string::npos)
			return MeshAccentDye;
		if (name.find("Garb") != std::string::npos || name.find("Cape") != std::string::npos)
			return dyed ? MeshAccentTrim | MeshAccentDye : MeshAccentTrim;
		return 0;
	}

	void CharacterLibrary::Attach(CharacterLook& look, const char* mesh, uint32_t joint, const glm::mat4& offset) const
	{
		if (const std::optional<uint32_t> index = Mesh(mesh))
			look.Props.push_back({ *index, joint, offset });
	}

	const CharacterLook& CharacterLibrary::ForPlayer(const PlayerLook& look) const
	{
		const uint64_t key = LookKey(look);
		if (const auto it = m_Players.find(key); it != m_Players.end())
			return it->second;

		CharacterLook result;
		ItemBase held = look.Weapon;
		if (m_Hero && held == ItemBase::Wand)
			held = look.Style.Cast == Focus::Staff ? ItemBase::Staff : look.Style.Cast == Focus::Hands ? ItemBase::Count : ItemBase::Wand;
		result.Pose = m_Hero && held == ItemBase::Staff ? Stance::Staff : PlayerStance(held);
		if (m_Hero)
			DressHero(result, look);
		else
			DressKayKit(result, look);
		const CharacterRig& rig = m_Rigs[result.Skeleton];
		const std::vector<uint32_t> weapons = WeaponMeshes(held);
		const glm::mat4 grip = GripOf(held);
		for (const uint32_t weapon : weapons)
			result.Props.push_back({ weapon, held == ItemBase::Bow ? rig.LeftHand : rig.RightHand, grip });
		if ((held == ItemBase::Wand || held == ItemBase::Staff) && !weapons.empty())
		{
			glm::vec3 low(1e9f);
			glm::vec3 high(-1e9f);
			for (const uint32_t weapon : weapons)
			{
				low = glm::min(low, m_Models.GetMesh(weapon).Min);
				high = glm::max(high, m_Models.GetMesh(weapon).Max);
			}
			const glm::vec3 span = high - low;
			const int axis = span.x >= span.y && span.x >= span.z ? 0 : span.y >= span.z ? 1 : 2;
			glm::vec3 a = (low + high) * 0.5f;
			glm::vec3 b = a;
			a[axis] = low[axis];
			b[axis] = high[axis];
			const glm::vec3 first = glm::vec3(grip * glm::vec4(a, 1.0f));
			const glm::vec3 second = glm::vec3(grip * glm::vec4(b, 1.0f));
			result.Tip = m_Hero ? second : glm::length(first) > glm::length(second) ? first : second;
			result.TipJoint = rig.RightHand;
		}
		else if (m_Hero && held == ItemBase::Count)
		{
			result.Tip = glm::vec3(0.0f, HandTip, 0.0f);
			result.TipJoint = rig.RightHand;
		}
		result.Height = TopOf(result) * result.Scale;
		return m_Players.emplace(key, std::move(result)).first->second;
	}

	void CharacterLibrary::DressHero(CharacterLook& look, const PlayerLook& player) const
	{
		const size_t outfit = size_t(player.Style.Body) < m_Outfits.size() && m_Outfits[size_t(player.Style.Body)] ? size_t(player.Style.Body) : size_t(Outfit::Wanderer);
		const std::string folder = OutfitModels[outfit].Folder;
		look.Skeleton = *m_Outfits[outfit];
		look.Scale = HeroScale;
		AddNode(look, folder + "Wizard_Garb");
		AddNode(look, folder + "Wizard_Cape", 1.0f, true);
		AddNode(look, folder + "Hero_Head", HoodShade);
	}

	void CharacterLibrary::DressKayKit(CharacterLook& look, const PlayerLook& player) const
	{
		look.Skeleton = m_Shared;
		look.Scale = PlayerScale;
		const std::string outfit = OutfitOf(player.Body);
		const std::string legs = OutfitOf(player.Feet);
		const std::string parts[] = { outfit + "_Body", outfit + "_ArmLeft", outfit + "_ArmRight", outfit + "_Cape", legs + "_LegLeft", legs + "_LegRight" };
		for (const std::string& part : parts)
			if (const std::optional<uint32_t> mesh = Mesh(part.c_str()))
				look.Parts.push_back({ *mesh });
		switch (player.Head)
		{
		case ItemBase::WizardHat:
			AddParts(look, { "Mage_Head", "Mage_Hat" });
			break;
		case ItemBase::Hood:
			AddParts(look, { "RogueHooded_Head", "RogueHooded_Mask" });
			break;
		case ItemBase::Helm:
			AddParts(look, { "Knight_Head", "Knight_Helmet" });
			break;
		default:
			if (const std::optional<uint32_t> mesh = Mesh((outfit + "_Head").c_str()))
				look.Parts.push_back({ *mesh });
			break;
		}
		if (player.Weapon == ItemBase::Bow || player.Weapon == ItemBase::Crossbow)
			AddParts(look, { "Ranger_Quiver" });
	}

	const CharacterLook* CharacterLibrary::ForEnemy(EntityKind kind) const
	{
		const auto it = m_Enemies.find(kind);
		return it != m_Enemies.end() && !it->second.Parts.empty() ? &it->second : nullptr;
	}

	std::vector<uint32_t> CharacterLibrary::WeaponMeshes(ItemBase base) const
	{
		const auto prefer = [&](const char* original, const char* fallback)
		{
			std::vector<uint32_t> meshes = NodeMeshes(original);
			return meshes.empty() ? NodeMeshes(fallback) : meshes;
		};
		switch (base)
		{
		case ItemBase::Wand:
			return prefer("Armory/Wand", "wand");
		case ItemBase::Staff:
			return prefer("Armory/Staff", "staff");
		case ItemBase::Sword:
			return NodeMeshes("sword_1handed");
		case ItemBase::Axe:
			return NodeMeshes("axe_1handed");
		case ItemBase::Dagger:
			return NodeMeshes("dagger");
		case ItemBase::Spear:
			return NodeMeshes("spear_A");
		case ItemBase::Bow:
			return NodeMeshes("bow_withString");
		case ItemBase::Crossbow:
			return NodeMeshes("crossbow_2handed");
		default:
			return {};
		}
	}

	std::vector<uint32_t> CharacterLibrary::ArmorMeshes(ItemBase base) const
	{
		std::vector<uint32_t> meshes;
		const auto add = [&](std::initializer_list<const char*> names)
		{
			for (const char* name : names)
				if (const std::optional<uint32_t> mesh = Mesh(name))
					meshes.push_back(*mesh);
		};
		switch (base)
		{
		case ItemBase::WizardHat:
			add({ "Mage_Hat" });
			break;
		case ItemBase::Hood:
			add({ "Skeleton_Rogue_Hood" });
			break;
		case ItemBase::Helm:
			add({ "Knight_Helmet" });
			break;
		case ItemBase::Robe:
			add({ "Mage_Body" });
			break;
		case ItemBase::Tunic:
			add({ "Ranger_Body" });
			break;
		case ItemBase::Plate:
			add({ "Knight_Body" });
			break;
		case ItemBase::Sandals:
			add({ "Mage_LegLeft", "Mage_LegRight" });
			break;
		case ItemBase::Boots:
			add({ "Ranger_LegLeft", "Ranger_LegRight" });
			break;
		case ItemBase::Greaves:
			add({ "Knight_LegLeft", "Knight_LegRight" });
			break;
		default:
			break;
		}
		return meshes;
	}

	std::optional<uint32_t> CharacterLibrary::ArrowMesh(AbilityId ability) const
	{
		switch (ability)
		{
		case AbilityId::Arrow:
		case AbilityId::Volley:
		case AbilityId::PiercingShot:
			return Mesh("arrow_bow");
		case AbilityId::Quarrel:
			return Mesh("arrow_crossbow");
		default:
			return std::nullopt;
		}
	}

	Stance PlayerStance(ItemBase weapon)
	{
		switch (weapon)
		{
		case ItemBase::Spear:
		case ItemBase::Staff:
			return Stance::Heavy;
		case ItemBase::Bow:
			return Stance::Bow;
		case ItemBase::Crossbow:
			return Stance::Rifle;
		default:
			return Stance::Light;
		}
	}

	AttackAnim PlayerAttackAnim(AbilityId ability)
	{
		switch (ability)
		{
		case AbilityId::Slash:
			return { Anim::Slash };
		case AbilityId::Chop:
			return { Anim::Chop };
		case AbilityId::Stab:
			return { Anim::Stab };
		case AbilityId::Thrust:
			return { Anim::HeavyStab };
		case AbilityId::GroundSlam:
			return { Anim::HeavyChop };
		case AbilityId::Cyclone:
			return { Anim::Spinning };
		case AbilityId::Bolt:
		case AbilityId::Orb:
		case AbilityId::ChainLightning:
		case AbilityId::EmberBolt:
			return { .Windup = Anim::CastFlick, .Variants = 3 };
		case AbilityId::FrostSpear:
			return { .Windup = Anim::CastHurl, .Hold = true };
		case AbilityId::PurpleFireball:
			return { .Windup = Anim::CastSurge, .Hold = true };
		case AbilityId::ObsidianBlades:
			return { Anim::CastSlash };
		case AbilityId::HexShield:
			return { .Windup = Anim::CastWard, .Hold = true, .HoldAt = 1.25f, .HoldReach = 0.14f };
		case AbilityId::IceSpear:
			return { Anim::CastLunge };
		case AbilityId::Fireball:
		case AbilityId::ArcLightning:
			return { Anim::CastHurl };
		case AbilityId::ChainHook:
			return { Anim::CastLash };
		case AbilityId::GravityWell:
			return { Anim::CastGrasp };
		case AbilityId::Firebomb:
			return { Anim::CastLob };
		case AbilityId::StormStrike:
		case AbilityId::EarthSpike:
			return { Anim::CastSmite };
		case AbilityId::Blink:
		case AbilityId::Warp:
			return { Anim::CastBlink };
		case AbilityId::Meteor:
		case AbilityId::Comet:
			return { Anim::CastSummon };
		case AbilityId::GlacialNova:
		case AbilityId::FrostNova:
			return { Anim::CastNova };
		case AbilityId::DiamondWard:
			return { Anim::CastWard };
		case AbilityId::Rejuvenate:
			return { Anim::UseItem };
		case AbilityId::Arrow:
		case AbilityId::Volley:
		case AbilityId::PiercingShot:
			return { Anim::BowDraw, Anim::BowRelease };
		case AbilityId::RainOfArrows:
			return { Anim::BowDrawUp, Anim::BowReleaseUp };
		case AbilityId::Quarrel:
			return { Anim::RifleAim, Anim::RifleShoot };
		default:
			return {};
		}
	}

	AttackAnim EnemyAttackAnim(EntityKind kind)
	{
		switch (kind)
		{
		case EntityKind::Chaser:
			return { Anim::Punch };
		case EntityKind::Shooter:
			return { Anim::RifleAim, Anim::RifleShoot };
		case EntityKind::Brute:
			return { Anim::HeavyChop };
		case EntityKind::Caster:
			return { Anim::MagicRaise, Anim::MagicShoot };
		case EntityKind::Archivist:
			return { Anim::MagicCast, Anim::Count, true };
		default:
			return {};
		}
	}

	glm::mat4 ModelTransform(const glm::vec3& base, float facing, float scale)
	{
		const glm::vec3 forward(std::cos(facing), 0.0f, std::sin(facing));
		const glm::vec3 right(std::sin(facing), 0.0f, -std::cos(facing));
		glm::mat4 transform(1.0f);
		transform[0] = glm::vec4(right * scale, 0.0f);
		transform[1] = glm::vec4(0.0f, scale, 0.0f, 0.0f);
		transform[2] = glm::vec4(forward * scale, 0.0f);
		transform[3] = glm::vec4(base, 1.0f);
		return transform;
	}

	CharacterPose DrawCharacterModel(WorldRenderer& renderer, const CharacterLibrary& library, const CharacterLook& look, const CharacterAnimator& animator, const glm::vec3& base, float facing, float scale, const glm::vec4& tint, float emission, const glm::vec4& accent, const glm::vec4& flash, const ModelDissolve& dissolve)
	{
		CharacterPose pose;
		pose.Model = ModelTransform(base, facing, scale);
		animator.Evaluate(library, look.Skeleton, pose.Globals);
		const ModelSkeleton& skeleton = library.GetModels().GetSkeleton(look.Skeleton);
		JointMatrices skin;
		ComputeSkin(skeleton, pose.Globals, skin);
		const uint32_t palette = renderer.AddPalette(skin, skeleton.JointCount());
		const bool accented = glm::vec3(accent) != glm::vec3(0.0f);
		for (const CharacterPart& part : look.Parts)
			renderer.DrawModel(part.Mesh, pose.Model, palette, glm::vec4(glm::vec3(tint) * part.Shade, tint.a), emission, base.y, accent, accented ? part.Accent : 0u, flash, dissolve);
		for (const PropAttachment& prop : look.Props)
			renderer.DrawModel(prop.Mesh, pose.Model * pose.Globals[prop.Joint] * prop.Offset, NoPalette, tint, emission, base.y, accent, 0u, flash, dissolve);
		return pose;
	}
}

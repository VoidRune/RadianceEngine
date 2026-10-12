#include "SoundBank.h"
#include <string>
#include <vector>

namespace Game
{
	namespace
	{
		constexpr const char* AudioRoot = "res/Audio/";

		struct CueDef
		{
			Cue Id;
			const char* Path;
			uint8_t Variants = 0;
			CueBus Bus = CueBus::World;
			Reach Spatial = Reach::Medium;
			float Volume = 1.0f;
			float PitchVariation = 0.0f;
			float VolumeVariation = 0.0f;
			uint8_t MaxInstances = 4;
			float Reverb = 0.0f;
			uint8_t Priority = 128;
			bool Loop = false;
			bool Compressed = false;
		};

		using enum CueBus;
		using enum Reach;

		constexpr CueDef CueDefs[] = {
			{ Cue::SwordSwing, "Sfx/Weapon/SwordSwing", 3, World, Medium, 0.7f, 1.5f, 0.1f, 6, 0.35f, 150 },
			{ Cue::HeavySwing, "Sfx/Weapon/HeavySwing", 3, World, Medium, 0.75f, 1.5f, 0.1f, 6, 0.35f, 150 },
			{ Cue::DaggerSwing, "Sfx/Weapon/DaggerSwing", 3, World, Medium, 0.6f, 1.5f, 0.1f, 6, 0.3f, 150 },
			{ Cue::BowDraw, "Sfx/Weapon/BowDraw", 2, World, Close, 0.55f, 0.8f, 0.05f, 4, 0.2f, 140 },
			{ Cue::BowShoot, "Sfx/Weapon/BowShoot", 2, World, Medium, 0.75f, 1.2f, 0.1f, 8, 0.35f, 150 },
			{ Cue::ArrowHit, "Sfx/Weapon/ArrowHit", 2, World, Medium, 0.55f, 2.0f, 0.15f, 8, 0.3f, 120 },
			{ Cue::MetalHit, "Sfx/Weapon/MetalHit", 3, World, Medium, 0.55f, 1.5f, 0.15f, 6, 0.4f, 130 },
			{ Cue::HeavyHit, "Sfx/Weapon/HeavyHit", 3, World, Medium, 0.65f, 1.0f, 0.1f, 4, 0.45f, 160 },
			{ Cue::DaggerHit, "Sfx/Weapon/DaggerHit", 3, World, Medium, 0.5f, 1.5f, 0.15f, 6, 0.35f, 130 },
			{ Cue::Latch, "Sfx/Weapon/Latch", 0, World, Close, 0.8f, 1.0f, 0.1f, 4, 0.2f, 120 },
			{ Cue::Click, "Sfx/Weapon/Click", 0, World, Close, 0.9f, 1.0f, 0.1f, 4, 0.2f, 120 },
			{ Cue::FireBuildup, "Sfx/Spell/FireBuildup", 0, World, Medium, 0.6f, 0.5f, 0.0f, 6, 0.4f, 150 },
			{ Cue::FireLaunch, "Sfx/Spell/FireLaunch", 3, World, Medium, 0.8f, 0.8f, 0.05f, 6, 0.45f, 160 },
			{ Cue::FireImpact, "Sfx/Spell/FireImpact", 3, World, Wide, 0.85f, 1.0f, 0.05f, 6, 0.55f, 170 },
			{ Cue::IceBuildup, "Sfx/Spell/IceBuildup", 0, World, Medium, 0.55f, 0.5f, 0.0f, 6, 0.4f, 150 },
			{ Cue::IceLaunch, "Sfx/Spell/IceLaunch", 3, World, Medium, 0.7f, 0.8f, 0.05f, 6, 0.45f, 160 },
			{ Cue::IceImpact, "Sfx/Spell/IceImpact", 3, World, Medium, 0.75f, 1.0f, 0.05f, 6, 0.5f, 160 },
			{ Cue::ShockBuildup, "Sfx/Spell/ShockBuildup", 0, World, Medium, 0.5f, 0.5f, 0.0f, 6, 0.4f, 150 },
			{ Cue::ShockLaunch, "Sfx/Spell/ShockLaunch", 3, World, Medium, 0.65f, 1.0f, 0.05f, 8, 0.45f, 160 },
			{ Cue::ShockHit, "Sfx/Spell/ShockHit", 3, World, Medium, 0.65f, 1.0f, 0.05f, 8, 0.5f, 160 },
			{ Cue::Heal, "Sfx/Spell/Heal", 2, World, Medium, 0.6f, 0.0f, 0.0f, 3, 0.5f, 170 },
			{ Cue::Shield, "Sfx/Spell/Shield", 2, World, Medium, 0.6f, 0.5f, 0.0f, 3, 0.5f, 160 },
			{ Cue::TeleportOut, "Sfx/Spell/TeleportOut", 0, World, Medium, 0.7f, 0.8f, 0.0f, 4, 0.5f, 170 },
			{ Cue::TeleportIn, "Sfx/Spell/TeleportIn", 0, World, Medium, 0.7f, 0.8f, 0.0f, 4, 0.5f, 170 },
			{ Cue::ManaStart, "Sfx/Spell/ManaStart", 0, World, Medium, 0.55f, 0.5f, 0.0f, 4, 0.4f, 150 },
			{ Cue::ManaEnd, "Sfx/Spell/ManaEnd", 0, World, Medium, 0.55f, 0.5f, 0.0f, 4, 0.4f, 150 },
			{ Cue::Explosion, "Sfx/Impact/Explosion", 2, World, Wide, 1.0f, 1.5f, 0.05f, 4, 0.7f, 200 },
			{ Cue::Thunder, "Sfx/Impact/Thunder", 3, World, Wide, 0.75f, 1.0f, 0.0f, 2, 0.6f, 190 },
			{ Cue::Rumble, "Sfx/Impact/Rumble", 0, World, Wide, 0.8f, 1.0f, 0.0f, 3, 0.5f, 180 },
			{ Cue::Rock, "Sfx/Impact/Rock", 5, World, Medium, 0.8f, 2.0f, 0.15f, 8, 0.5f, 150 },
			{ Cue::Wood, "Sfx/Impact/Wood", 3, World, Medium, 1.0f, 1.5f, 0.1f, 6, 0.5f, 150 },
			{ Cue::Plank, "Sfx/Impact/Plank", 2, World, Medium, 0.7f, 1.5f, 0.1f, 4, 0.5f, 150 },
			{ Cue::Glass, "Sfx/Impact/Glass", 4, World, Medium, 1.0f, 2.0f, 0.1f, 6, 0.5f, 140 },
			{ Cue::GlassLight, "Sfx/Impact/GlassLight", 3, World, Close, 0.9f, 2.0f, 0.15f, 6, 0.4f, 120 },
			{ Cue::Body, "Sfx/Impact/Body", 5, World, Medium, 0.6f, 2.0f, 0.15f, 8, 0.3f, 150 },
			{ Cue::BodyLight, "Sfx/Impact/BodyLight", 4, World, Medium, 0.5f, 2.0f, 0.15f, 8, 0.3f, 140 },
			{ Cue::Soft, "Sfx/Impact/Soft", 3, World, Medium, 0.55f, 2.0f, 0.1f, 6, 0.3f, 130 },
			{ Cue::Plate, "Sfx/Impact/Plate", 3, World, Medium, 0.87f, 1.5f, 0.15f, 4, 0.35f, 160 },
			{ Cue::PlateLight, "Sfx/Impact/PlateLight", 3, World, Medium, 0.95f, 1.5f, 0.15f, 6, 0.35f, 140 },
			{ Cue::Bone, "Sfx/Impact/Bone", 7, World, Close, 0.9f, 2.5f, 0.2f, 10, 0.25f, 110 },
			{ Cue::Bell, "Sfx/Impact/Bell", 0, World, Medium, 0.75f, 1.0f, 0.0f, 3, 0.4f, 120 },
			{ Cue::WhooshLight, "Sfx/Whoosh/Light", 4, World, Close, 0.5f, 2.0f, 0.1f, 6, 0.2f, 130 },
			{ Cue::WhooshHeavy, "Sfx/Whoosh/Heavy", 4, World, Medium, 0.6f, 2.0f, 0.1f, 6, 0.3f, 140 },
			{ Cue::GhoulGrowl, "Sfx/Monster/GhoulGrowl", 6, World, Medium, 0.6f, 1.5f, 0.1f, 4, 0.45f, 130 },
			{ Cue::GhoulSnarl, "Sfx/Monster/GhoulSnarl", 6, World, Medium, 0.6f, 1.5f, 0.1f, 5, 0.4f, 140 },
			{ Cue::SkeletonHiss, "Sfx/Monster/Skeleton", 5, World, Medium, 0.55f, 1.5f, 0.1f, 4, 0.45f, 130 },
			{ Cue::Rattle, "Sfx/Monster/Rattle", 3, World, Close, 0.45f, 2.0f, 0.15f, 4, 0.3f, 110 },
			{ Cue::Wraith, "Sfx/Monster/Wraith", 7, World, Wide, 0.75f, 1.0f, 0.0f, 3, 0.6f, 190 },
			{ Cue::OgreRoar, "Sfx/Monster/OgreRoar", 4, World, Wide, 0.8f, 1.0f, 0.0f, 3, 0.5f, 180 },
			{ Cue::Grunt, "Sfx/Monster/Grunt", 6, World, Medium, 0.55f, 1.5f, 0.1f, 4, 0.4f, 130 },
			{ Cue::Growl, "Sfx/Monster/Growl", 4, World, Medium, 0.6f, 1.5f, 0.1f, 4, 0.45f, 130 },
			{ Cue::Spit, "Sfx/Monster/Spit", 3, World, Medium, 0.55f, 1.5f, 0.1f, 4, 0.4f, 140 },
			{ Cue::Collapse, "Sfx/Monster/Collapse", 3, World, Medium, 0.7f, 1.5f, 0.1f, 6, 0.45f, 150 },
			{ Cue::WyrmRoar, "Sfx/Monster/WyrmRoar", 0, World, Wide, 1.0f, 0.5f, 0.0f, 2, 0.6f, 220 },
			{ Cue::DemonCall, "Sfx/Monster/DemonCall", 0, World, Wide, 0.9f, 0.5f, 0.0f, 2, 0.6f, 220 },
			{ Cue::DemonGrowl, "Sfx/Monster/DemonGrowl", 0, World, Wide, 0.8f, 1.0f, 0.0f, 2, 0.5f, 200 },
			{ Cue::BeastDeath, "Sfx/Monster/BeastDeath", 0, World, Wide, 0.8f, 1.0f, 0.0f, 2, 0.5f, 200 },
			{ Cue::SpiritShout, "Sfx/Monster/SpiritShout", 0, World, Wide, 0.75f, 1.0f, 0.0f, 2, 0.5f, 190 },
			{ Cue::StepStone, "Sfx/Player/Step_Stone", 5, World, Close, 0.72f, 1.5f, 0.25f, 8, 0.2f, 60 },
			{ Cue::StepGrass, "Sfx/Player/Step_Grass", 5, World, Close, 0.65f, 1.5f, 0.25f, 8, 0.15f, 60 },
			{ Cue::StepWood, "Sfx/Player/Step_Wood", 5, World, Close, 0.43f, 1.5f, 0.25f, 8, 0.2f, 60 },
			{ Cue::StepCarpet, "Sfx/Player/Step_Carpet", 4, World, Close, 0.54f, 1.5f, 0.25f, 8, 0.1f, 60 },
			{ Cue::StepSand, "Sfx/Player/Step_Sand", 5, World, Close, 0.34f, 1.5f, 0.25f, 8, 0.15f, 60 },
			{ Cue::StepDirt, "Sfx/Player/Step_Dirt", 5, World, Close, 0.37f, 1.5f, 0.25f, 8, 0.2f, 60 },
			{ Cue::Cloth, "Sfx/Player/Cloth", 4, World, Close, 0.78f, 1.5f, 0.15f, 4, 0.15f, 100 },
			{ Cue::Coins, "Sfx/Item/Coins", 2, World, Close, 1.0f, 1.5f, 0.1f, 3, 0.15f, 120 },
			{ Cue::Leather, "Sfx/Item/Leather", 2, World, Close, 0.74f, 1.5f, 0.1f, 3, 0.15f, 110 },
			{ Cue::Drop, "Sfx/Item/Drop", 0, World, Close, 0.5f, 1.5f, 0.1f, 3, 0.2f, 110 },
			{ Cue::Belt, "Sfx/Item/Belt", 2, World, Close, 0.96f, 1.5f, 0.1f, 3, 0.15f, 110 },
			{ Cue::Unsheathe, "Sfx/Item/Unsheathe", 3, World, Close, 0.45f, 1.0f, 0.1f, 3, 0.25f, 120 },
			{ Cue::Creak, "Sfx/Item/Creak", 2, World, Medium, 0.5f, 1.0f, 0.1f, 3, 0.3f, 120 },
			{ Cue::BookOpen, "Sfx/Item/BookOpen", 0, Interface, Flat, 0.92f, 1.0f, 0.0f, 2, 0.0f, 120 },
			{ Cue::BookClose, "Sfx/Item/BookClose", 0, Interface, Flat, 0.95f, 1.0f, 0.0f, 2, 0.0f, 120 },
			{ Cue::BookFlip, "Sfx/Item/BookFlip", 3, World, Medium, 1.0f, 2.0f, 0.1f, 6, 0.3f, 120 },
			{ Cue::DoorOpen, "Sfx/Item/DoorOpen", 2, World, Medium, 0.6f, 1.0f, 0.05f, 3, 0.4f, 150 },
			{ Cue::DoorClose, "Sfx/Item/DoorClose", 0, World, Medium, 0.55f, 1.0f, 0.05f, 3, 0.4f, 150 },
			{ Cue::Anvil, "Sfx/Craft/Anvil", 6, World, Medium, 0.55f, 1.0f, 0.1f, 4, 0.4f, 150 },
			{ Cue::Crow, "Sfx/World/Crow", 5, World, Wide, 0.35f, 1.0f, 0.2f, 2, 0.4f, 60 },
			{ Cue::Gong, "Sfx/World/Gong", 0, Stinger, Flat, 0.6f, 0.0f, 0.0f, 2, 0.0f, 230 },
			{ Cue::Horn, "Sfx/World/Horn", 0, Stinger, Flat, 0.6f, 0.5f, 0.0f, 2, 0.0f, 230 },
			{ Cue::Drum, "Sfx/World/Drum", 0, Stinger, Flat, 1.0f, 0.0f, 0.0f, 2, 0.0f, 230 },
			{ Cue::UiClick, "Sfx/UI/Click", 2, Interface, Flat, 0.3f, 0.0f, 0.05f, 4, 0.0f, 200 },
			{ Cue::UiConfirm, "Sfx/UI/Confirm", 0, Interface, Flat, 0.45f, 0.0f, 0.0f, 2, 0.0f, 200 },
			{ Cue::UiError, "Sfx/UI/Error", 0, Interface, Flat, 0.45f, 0.0f, 0.0f, 2, 0.0f, 200 },
			{ Cue::UiNotify, "Sfx/UI/Notify", 0, Interface, Flat, 0.4f, 0.0f, 0.0f, 2, 0.0f, 200 },
			{ Cue::UiReward, "Sfx/UI/Reward", 0, Interface, Flat, 0.5f, 0.0f, 0.0f, 2, 0.0f, 210 },
			{ Cue::UiRare, "Sfx/UI/Rare", 0, World, Medium, 0.5f, 0.0f, 0.0f, 3, 0.3f, 190 },
			{ Cue::UiSuccess, "Sfx/UI/Success", 0, Interface, Flat, 0.5f, 0.0f, 0.0f, 2, 0.0f, 200 },
			{ Cue::UiFail, "Sfx/UI/Fail", 0, Interface, Flat, 0.5f, 0.0f, 0.0f, 2, 0.0f, 200 },
			{ Cue::UiHover, "Sfx/UI/Hover", 2, Interface, Flat, 0.2f, 0.0f, 0.1f, 6, 0.3f, 180 },
			{ Cue::UiOpen, "Sfx/UI/Open", 0, Interface, Flat, 0.4f, 0.0f, 0.0f, 2, 0.35f, 200 },
			{ Cue::UiClose, "Sfx/UI/Close", 0, Interface, Flat, 0.35f, 0.0f, 0.0f, 2, 0.3f, 200 },
			{ Cue::UiShatter, "Sfx/UI/Shatter", 0, Interface, Flat, 0.35f, 0.3f, 0.05f, 2, 0.4f, 200 },
			{ Cue::UiRift, "Sfx/UI/Rift", 0, Interface, Flat, 0.55f, 0.0f, 0.0f, 1, 0.45f, 210 },
			{ Cue::FireLoop, "Ambience/FireLoop", 0, World, Medium, 0.5f, 0.5f, 0.0f, 6, 0.4f, 150, true },
			{ Cue::ShockLoop, "Ambience/ShockLoop", 0, World, Medium, 0.45f, 0.5f, 0.0f, 6, 0.4f, 150, true },
			{ Cue::IceLoop, "Ambience/IceLoop", 0, World, Medium, 0.45f, 0.5f, 0.0f, 6, 0.4f, 150, true },
			{ Cue::Chant, "Ambience/Chant", 0, World, Medium, 0.6f, 0.5f, 0.0f, 4, 0.5f, 160, true },
			{ Cue::GatewayHum, "Ambience/Gateway", 0, World, Medium, 0.5f, 0.0f, 0.0f, 3, 0.4f, 100, true },
			{ Cue::Campfire, "Ambience/Campfire", 0, World, Close, 0.55f, 0.0f, 0.0f, 8, 0.2f, 90, true, true },
			{ Cue::Heartbeat, "Ambience/Heartbeat", 0, Interface, Flat, 0.45f, 0.0f, 0.0f, 1, 0.0f, 220, true },
		};

		constexpr bool CueDefsMatchIds()
		{
			for (size_t i = 0; i < std::size(CueDefs); i++)
				if (size_t(CueDefs[i].Id) != i)
					return false;
			return std::size(CueDefs) == size_t(Cue::Count);
		}
		static_assert(CueDefsMatchIds());

		Timbre::Bus BusFor(const AudioBuses& buses, CueBus bus)
		{
			switch (bus)
			{
			case CueBus::Interface:
				return buses.Interface;
			case CueBus::Stinger:
				return buses.Stingers;
			default:
				return buses.World;
			}
		}
	}

	SoundBank::SoundBank(Timbre::AudioEngine& engine, const AudioBuses& buses)
	{
		for (const CueDef& def : CueDefs)
		{
			Timbre::SoundDesc desc;
			desc.Mode = def.Compressed ? Timbre::LoadMode::Compressed : Timbre::LoadMode::Decompress;
			desc.Async = true;
			desc.Bus = BusFor(buses, def.Bus);
			desc.Volume = def.Volume;
			desc.Looping = def.Loop;
			desc.PitchVariation = def.PitchVariation;
			desc.VolumeVariation = def.VolumeVariation;
			desc.Priority = def.Priority;
			desc.MaxInstances = def.MaxInstances;
			desc.LimitMode = def.Loop ? Timbre::InstanceLimitMode::RejectNew : Timbre::InstanceLimitMode::StealOldest;
			desc.Spatial = MakeSpatial(def.Spatial);
			if (def.Reverb > 0.0f && buses.Reverb)
				desc.Sends.push_back({ buses.Reverb, def.Reverb });

			std::vector<Timbre::Sound> sounds;
			const std::string base = std::string(AudioRoot) + def.Path;
			if (def.Variants == 0)
				sounds.push_back(engine.LoadSound(base + ".ogg", desc));
			for (uint8_t i = 1; i <= def.Variants; i++)
				sounds.push_back(engine.LoadSound(base + "_" + std::to_string(i) + ".ogg", desc));
			std::erase_if(sounds, [](const Timbre::Sound& sound) { return !sound; });
			if (!sounds.empty())
				m_Cues[size_t(def.Id)] = Timbre::SoundContainer(std::move(sounds), Timbre::ContainerMode::Random);
		}
	}

	const char* SoundBank::GetPath(Cue cue)
	{
		return size_t(cue) < std::size(CueDefs) ? CueDefs[size_t(cue)].Path : "";
	}

	Timbre::SpatialSettings SoundBank::MakeSpatial(Reach reach)
	{
		Timbre::SpatialSettings spatial;
		spatial.Model = Timbre::AttenuationModel::Custom;
		spatial.MinDistance = ListenerDistance;
		spatial.DopplerFactor = 0.0f;
		switch (reach)
		{
		case Reach::Close:
			spatial.Curve = { { 7.0f, 1.0f }, { 9.5f, 0.7f }, { 14.0f, 0.32f }, { 20.0f, 0.1f }, { 26.0f, 0.0f } };
			spatial.AirAbsorption = 1.5f;
			break;
		case Reach::Wide:
			spatial.Curve = { { 7.0f, 1.0f }, { 14.0f, 0.8f }, { 26.0f, 0.45f }, { 40.0f, 0.18f }, { 60.0f, 0.0f } };
			spatial.AirAbsorption = 2.0f;
			break;
		default:
			spatial.Curve = { { 7.0f, 1.0f }, { 10.5f, 0.8f }, { 16.0f, 0.45f }, { 24.0f, 0.18f }, { 34.0f, 0.05f }, { 45.0f, 0.0f } };
			spatial.AirAbsorption = 2.5f;
			break;
		}
		return spatial;
	}

	Timbre::Voice SoundBank::Play(Cue cue, const Timbre::PlayParams& params) const
	{
		const Timbre::SoundContainer& container = m_Cues[size_t(cue)];
		return container ? container.Play(params) : Timbre::Voice{};
	}
}

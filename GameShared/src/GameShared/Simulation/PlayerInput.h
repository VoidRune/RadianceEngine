#pragma once
#include "GameShared/Core/Math.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Game
{
	enum class InputButton : uint8_t
	{
		Primary = 1 << 0,
		Skill1 = 1 << 1,
		Skill2 = 1 << 2,
		Skill3 = 1 << 3,
		Dodge = 1 << 4,
	};

	constexpr uint8_t KnownInputButtons = 0x1F;
	constexpr float MaxAimDistance = 60.0f;

	inline InputButton SlotButton(uint32_t slot)
	{
		constexpr InputButton Buttons[] = { InputButton::Primary, InputButton::Skill1, InputButton::Skill2, InputButton::Skill3 };
		return Buttons[std::min<uint32_t>(slot, 3)];
	}

	inline float QuantizeAimDistance(float distance)
	{
		return std::round(std::clamp(distance, 0.0f, MaxAimDistance) * 32.0f) / 32.0f;
	}

	struct PlayerInput
	{
		uint32_t Sequence = 0;
		Vec2 Move;
		float Aim = 0.0f;
		float AimDistance = 0.0f;
		uint8_t Buttons = 0;
		uint8_t Interact = 0;
		uint32_t ViewTick = 0;
		uint8_t ViewFraction = 0;

		bool IsDown(InputButton button) const { return (Buttons & uint8_t(button)) != 0; }
		double GetViewTime() const { return double(ViewTick) + double(ViewFraction) / 256.0; }
	};
}

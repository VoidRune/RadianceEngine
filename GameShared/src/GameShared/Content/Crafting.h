#pragma once
#include "GameShared/Content/Items.h"
#include "GameShared/Content/Progression.h"
#include "GameShared/Core/Random.h"
#include <cstdint>
#include <optional>
#include <string>

namespace Game
{
	enum class ForgeAction : uint8_t
	{
		Temper,
		Hone,
		Imbue,
		Reforge,
		Ascend,
		Count,
	};

	enum class GemAction : uint8_t
	{
		Empower,
		Polish,
		Corrupt,
		Count,
	};

	struct CraftResult
	{
		bool Applied = false;
		bool Success = false;
		std::string Message;
	};

	const char* GetForgeActionName(ForgeAction action);
	const char* DescribeForgeAction(ForgeAction action);
	const char* GetGemActionName(GemAction action);
	const char* DescribeGemAction(GemAction action);

	MaterialStock GetForgeCost(const Item& item, ForgeAction action, uint32_t affix);
	std::optional<std::string> CheckForge(const Item& item, ForgeAction action, uint32_t affix, StatCategory category);
	float GetHoneChance(const Item& item, uint32_t affix);
	CraftResult ApplyForge(Random& random, Item& item, ForgeAction action, uint32_t affix, StatCategory category);

	MaterialStock GetGemCost(const Item& gem, GemAction action);
	std::optional<std::string> CheckGemCraft(const Item& gem, GemAction action);
	CraftResult ApplyGemCraft(Random& random, Item& gem, GemAction action);

	MaterialStock GetSalvageValue(const Item& item);
	bool CanAfford(const MaterialStock& stock, const MaterialStock& cost);
	void Spend(MaterialStock& stock, const MaterialStock& cost);
	void Gain(MaterialStock& stock, const MaterialStock& gain);
}

#include "PropLibrary.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace Game
{
	namespace
	{
		using Json = nlohmann::json;

		struct StoredProp
		{
			std::vector<PropCollider> Colliders;
			std::vector<PropPart> Parts;
			bool Tall = false;
		};

		struct Library
		{
			std::array<StoredProp, size_t(PropId::Count)> Stored;
			std::array<PropDef, size_t(PropId::Count)> Views;
			uint32_t Hash = 0;
			uint32_t Custom = 0;
		};

		constexpr const char* MeshNames[] = { "box", "cylinder", "cylinderX", "cone", "sphere", "wedge", "roof" };
		static_assert(std::size(MeshNames) == size_t(MeshKind::Count));

		void Mix(uint32_t& hash, const void* data, size_t size)
		{
			const uint8_t* bytes = static_cast<const uint8_t*>(data);
			for (size_t i = 0; i < size; i++)
				hash = (hash ^ bytes[i]) * 16777619u;
		}

		template<typename T>
		void Mix(uint32_t& hash, const T& value)
		{
			Mix(hash, &value, sizeof(T));
		}

		void Refresh(Library& library)
		{
			uint32_t hash = 2166136261u;
			for (size_t i = 0; i < library.Stored.size(); i++)
			{
				const StoredProp& stored = library.Stored[i];
				library.Views[i] = { PropId(i), Props[i].Name, stored.Colliders, stored.Parts, stored.Tall };
				Mix(hash, uint32_t(i));
				Mix(hash, uint8_t(stored.Tall));
				for (const PropCollider& collider : stored.Colliders)
				{
					Mix(hash, collider.Kind);
					Mix(hash, collider.Offset);
					Mix(hash, collider.Half);
					Mix(hash, collider.Radius);
					Mix(hash, collider.Yaw);
					Mix(hash, collider.Blocks);
				}
				Mix(hash, uint32_t(stored.Parts.size()));
			}
			library.Hash = hash;
		}

		void Reset(Library& library)
		{
			for (size_t i = 0; i < std::size(Props); i++)
			{
				StoredProp& stored = library.Stored[i];
				stored.Colliders.assign(Props[i].Colliders.begin(), Props[i].Colliders.end());
				stored.Parts.assign(Props[i].Parts.begin(), Props[i].Parts.end());
				stored.Tall = Props[i].Tall;
			}
			library.Custom = 0;
			Refresh(library);
		}

		Library& GetLibrary()
		{
			static Library library = []
			{
				Library created;
				Reset(created);
				return created;
			}();
			return library;
		}

		bool SameText(std::string_view a, std::string_view b)
		{
			return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return std::tolower(uint8_t(x)) == std::tolower(uint8_t(y)); });
		}

		double Round(float value)
		{
			return std::round(double(value) * 10000.0) / 10000.0;
		}

		Json ToJson(Vec2 value)
		{
			return Json::array({ Round(value.x), Round(value.y) });
		}

		std::string ColorText(uint32_t color)
		{
			char text[8];
			std::snprintf(text, sizeof(text), "#%06X", color & 0xFFFFFFu);
			return text;
		}

		const char* BlockName(Block blocks)
		{
			switch (blocks)
			{
			case Block::None:
				return "none";
			case Block::Movement:
				return "movement";
			case Block::Projectiles:
				return "projectiles";
			default:
				return "all";
			}
		}

		void Expect(bool condition, const std::string& message)
		{
			if (!condition)
				throw std::runtime_error(message);
		}

		float Number(const Json& value, const char* name, float fallback)
		{
			if (!value.contains(name))
				return fallback;
			Expect(value[name].is_number(), std::string("\"") + name + "\" must be a number");
			const float number = value[name].get<float>();
			Expect(std::isfinite(number), std::string("\"") + name + "\" must be finite");
			return number;
		}

		Vec2 Pair(const Json& value, const char* name, Vec2 fallback)
		{
			if (!value.contains(name))
				return fallback;
			const Json& pair = value[name];
			Expect(pair.is_array() && pair.size() == 2 && pair[0].is_number() && pair[1].is_number(), std::string("\"") + name + "\" must be [x, y]");
			return { pair[0].get<float>(), pair[1].get<float>() };
		}

		uint32_t ParseColor(const Json& value)
		{
			if (value.is_number_unsigned())
				return value.get<uint32_t>() & 0xFFFFFFu;
			Expect(value.is_string(), "\"color\" must be \"#RRGGBB\"");
			const std::string text = value.get<std::string>();
			Expect(text.size() == 7 && text[0] == '#', "\"color\" must be \"#RRGGBB\"");
			return uint32_t(std::stoul(text.substr(1), nullptr, 16));
		}

		Block ParseBlocks(const Json& value)
		{
			const std::string text = value.is_string() ? value.get<std::string>() : "all";
			if (SameText(text, "none"))
				return Block::None;
			if (SameText(text, "movement"))
				return Block::Movement;
			if (SameText(text, "projectiles"))
				return Block::Projectiles;
			Expect(SameText(text, "all"), "\"blocks\" must be all, movement, projectiles or none");
			return Block::All;
		}

		PropCollider ParseCollider(const Json& value)
		{
			Expect(value.is_object(), "colliders must be objects");
			PropCollider collider;
			const std::string shape = value.value("shape", std::string("box"));
			collider.Offset = Pair(value, "offset", {});
			collider.Blocks = ParseBlocks(value.value("blocks", Json("all")));
			if (SameText(shape, "circle"))
			{
				collider.Kind = ShapeKind::Circle;
				collider.Radius = Number(value, "radius", 0.0f);
				collider.Half = { collider.Radius, collider.Radius };
				Expect(collider.Radius > 0.0f, "a circle collider needs a positive \"radius\"");
				return collider;
			}
			Expect(SameText(shape, "box"), "\"shape\" must be circle or box");
			collider.Kind = ShapeKind::Box;
			collider.Half = Pair(value, "half", {});
			collider.Yaw = Number(value, "yaw", 0.0f);
			Expect(collider.Half.x > 0.0f && collider.Half.y > 0.0f, "a box collider needs a positive \"half\"");
			return collider;
		}

		PropPart ParsePart(const Json& value)
		{
			Expect(value.is_object(), "parts must be objects");
			PropPart part;
			const std::string mesh = value.value("mesh", std::string("box"));
			const auto found = std::find_if(std::begin(MeshNames), std::end(MeshNames), [&](const char* name) { return SameText(name, mesh); });
			Expect(found != std::end(MeshNames), "unknown \"mesh\" " + mesh);
			part.Mesh = MeshKind(found - std::begin(MeshNames));
			part.Offset = Pair(value, "offset", {});
			part.Base = Number(value, "base", 0.0f);
			part.Half = Pair(value, "half", { 0.5f, 0.5f });
			part.Height = Number(value, "height", 1.0f);
			part.Yaw = Number(value, "yaw", 0.0f);
			part.Emission = Number(value, "emission", 0.0f);
			if (value.contains("color"))
				part.Color = ParseColor(value["color"]);
			Expect(part.Half.x >= 0.0f && part.Half.y >= 0.0f && part.Height >= 0.0f, "part sizes can't be negative");
			return part;
		}
	}

	const PropDef& GetProp(PropId id)
	{
		const Library& library = GetLibrary();
		return library.Views[size_t(id) < library.Views.size() ? size_t(id) : 0];
	}

	bool LoadPropLibrary(const std::filesystem::path& path, std::string& error)
	{
		std::ifstream file(path);
		if (!file)
		{
			error = "can't open " + path.string();
			return false;
		}
		Library staged;
		Reset(staged);
		try
		{
			const Json root = Json::parse(file);
			Expect(root.is_object() && root.contains("props") && root["props"].is_array(), "the file needs a \"props\" array");
			for (const Json& entry : root["props"])
			{
				Expect(entry.is_object() && entry.contains("name") && entry["name"].is_string(), "every prop needs a \"name\"");
				const std::string name = entry["name"].get<std::string>();
				const auto known = std::find_if(std::begin(Props), std::end(Props), [&](const PropDef& def) { return SameText(def.Name, name); });
				Expect(known != std::end(Props), "unknown prop \"" + name + "\"");
				StoredProp& stored = staged.Stored[size_t(known->Id)];
				if (entry.contains("tall"))
					stored.Tall = entry["tall"].get<bool>();
				if (entry.contains("colliders"))
				{
					stored.Colliders.clear();
					for (const Json& collider : entry["colliders"])
						stored.Colliders.push_back(ParseCollider(collider));
				}
				if (entry.contains("parts"))
				{
					stored.Parts.clear();
					for (const Json& part : entry["parts"])
						stored.Parts.push_back(ParsePart(part));
				}
				staged.Custom++;
			}
		}
		catch (const std::exception& exception)
		{
			error = path.string() + ": " + exception.what();
			return false;
		}
		Library& library = GetLibrary();
		library.Stored = std::move(staged.Stored);
		library.Custom = staged.Custom;
		Refresh(library);
		return true;
	}

	bool SavePropLibrary(const std::filesystem::path& path, std::string& error)
	{
		const Library& library = GetLibrary();
		Json props = Json::array();
		for (size_t i = 0; i < library.Stored.size(); i++)
		{
			const StoredProp& stored = library.Stored[i];
			Json colliders = Json::array();
			for (const PropCollider& collider : stored.Colliders)
			{
				Json entry = { { "shape", collider.Kind == ShapeKind::Circle ? "circle" : "box" } };
				if (collider.Kind == ShapeKind::Circle)
					entry["radius"] = Round(collider.Radius);
				else
				{
					entry["half"] = ToJson(collider.Half);
					if (collider.Yaw != 0.0f)
						entry["yaw"] = Round(collider.Yaw);
				}
				if (collider.Offset.x != 0.0f || collider.Offset.y != 0.0f)
					entry["offset"] = ToJson(collider.Offset);
				entry["blocks"] = BlockName(collider.Blocks);
				colliders.push_back(entry);
			}
			Json parts = Json::array();
			for (const PropPart& part : stored.Parts)
			{
				Json entry = { { "mesh", MeshNames[size_t(part.Mesh)] }, { "offset", ToJson(part.Offset) }, { "base", Round(part.Base) }, { "half", ToJson(part.Half) }, { "height", Round(part.Height) }, { "color", ColorText(part.Color) } };
				if (part.Yaw != 0.0f)
					entry["yaw"] = Round(part.Yaw);
				if (part.Emission != 0.0f)
					entry["emission"] = Round(part.Emission);
				parts.push_back(entry);
			}
			props.push_back({ { "name", Props[i].Name }, { "tall", stored.Tall }, { "colliders", colliders }, { "parts", parts } });
		}
		const Json root = { { "version", 1 }, { "props", props } };

		std::error_code code;
		if (path.has_parent_path())
			std::filesystem::create_directories(path.parent_path(), code);
		std::ofstream file(path, std::ios::trunc);
		if (!file)
		{
			error = "can't write " + path.string();
			return false;
		}
		file << root.dump(1, '\t') << '\n';
		return bool(file);
	}

	void ResetPropLibrary()
	{
		Reset(GetLibrary());
	}

	uint32_t GetPropLibraryHash()
	{
		return GetLibrary().Hash;
	}

	uint32_t CountCustomProps()
	{
		return GetLibrary().Custom;
	}
}

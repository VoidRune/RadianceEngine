#pragma once
#include "UI/UiAssets.h"
#include "UI/UiTypes.h"
#include <string>
#include <string_view>
#include <vector>

namespace Game
{
	struct RectStyle
	{
		UiColor Fill{ 0.0f };
		UiColor FillBottom{ -1.0f };
		float Radius = 0.0f;
		UiColor Border{ 0.0f };
		float BorderWidth = 0.0f;
		float Softness = 0.0f;
		UiColor Glow{ 0.0f };
		float GlowSize = 8.0f;
		float Sweep = 0.0f;
		float Rotation = 0.0f;
		bool Horizontal = false;
	};

	struct TextStyle
	{
		UiFont Font = UiFont::Body;
		float Size = 16.0f;
		UiColor Color{ 1.0f };
		UiColor ColorBottom{ -1.0f };
		float Weight = 0.0f;
		float Outline = 0.0f;
		UiColor OutlineColor{ 0.0f, 0.0f, 0.0f, 0.85f };
		glm::vec2 ShadowOffset{ 0.0f, 1.0f };
		UiColor ShadowColor{ 0.0f };
		float ShadowBlur = 1.0f;
		float Tracking = 0.0f;
	};

	struct IconStyle
	{
		UiColor Color{ 1.0f };
		UiColor ColorBottom{ -1.0f };
		float Weight = 0.0f;
		float Outline = 0.0f;
		UiColor OutlineColor{ 0.0f, 0.0f, 0.0f, 0.85f };
		glm::vec2 ShadowOffset{ 0.0f, 1.5f };
		UiColor ShadowColor{ 0.0f };
		float ShadowBlur = 2.0f;
	};

	struct TextLine
	{
		std::string_view Text;
		float Width = 0.0f;
	};

	class UiDrawList
	{
	public:
		UiDrawList() = default;
		explicit UiDrawList(const UiAssets& assets) : m_Assets(&assets) {}

		void Reset(float scale);
		void PushClip(const UiRect& rect);
		void PopClip();
		UiRect GetClip() const;

		void Rect(const UiRect& rect, const RectStyle& style);
		void Shadow(const UiRect& rect, float radius, float blur, UiColor color, glm::vec2 offset = { 0.0f, 4.0f });
		void Circle(glm::vec2 center, float radius, UiColor fill, UiColor border = UiColor(0.0f), float borderWidth = 0.0f);
		void Line(glm::vec2 from, glm::vec2 to, float width, UiColor color);
		void Triangle(glm::vec2 center, glm::vec2 size, float rotation, UiColor color, UiColor border = UiColor(0.0f), float borderWidth = 0.0f);
		void Orb(glm::vec2 center, float radius, float fill, UiColor light, UiColor deep, UiColor glass, UiColor highlight, float phase, float wave = 0.035f);
		void Icon(UiIcon icon, const UiRect& rect, const IconStyle& style);
		void Image(UiImage image, const UiRect& rect, UiColor tint = UiColor(1.0f), float saturation = 1.0f, float radius = 0.0f);

		glm::vec2 Text(glm::vec2 topLeft, std::string_view text, const TextStyle& style);
		void TextAligned(const UiRect& rect, std::string_view text, const TextStyle& style, glm::vec2 align = { 0.5f, 0.5f });
		float TextWrapped(const UiRect& rect, std::string_view text, const TextStyle& style, float align = 0.0f, float lineSpacing = 1.0f);
		void TextFit(const UiRect& rect, std::string_view text, const TextStyle& style, glm::vec2 align = { 0.0f, 0.5f });

		float MeasureWidth(std::string_view text, const TextStyle& style) const;
		float LineHeight(const TextStyle& style) const;
		float CapHeight(const TextStyle& style) const;
		std::vector<TextLine> WrapText(std::string_view text, const TextStyle& style, float width) const;
		std::string Ellipsize(std::string_view text, const TextStyle& style, float width) const;

		const std::vector<UiPrimitive>& GetPrimitives() const { return m_Primitives; }
		const UiAssets& GetAssets() const { return *m_Assets; }

	private:
		UiPrimitive& Add(UiMode mode, glm::vec2 center, glm::vec2 halfSize);
		void Glyphs(glm::vec2 pen, std::string_view text, const TextStyle& style, UiColor top, UiColor bottom, glm::vec2 offset, float outline, float softness);
		float Snap(float value) const;

		const UiAssets* m_Assets = nullptr;
		std::vector<UiPrimitive> m_Primitives;
		std::vector<UiRect> m_Clips;
		float m_Scale = 1.0f;
	};

	uint32_t DecodeUtf8(std::string_view text, size_t& index);
}

#include "UiDrawList.h"
#include <cmath>

namespace Game
{
	namespace
	{
		UiColor Resolve(const UiColor& bottom, const UiColor& top)
		{
			return bottom.a < 0.0f ? top : bottom;
		}

		bool IsSpace(uint32_t codepoint)
		{
			return codepoint == ' ' || codepoint == '\t';
		}
	}

	uint32_t DecodeUtf8(std::string_view text, size_t& index)
	{
		const uint8_t lead = uint8_t(text[index++]);
		if (lead < 0x80)
			return lead;
		uint32_t extra = lead >= 0xF0 ? 3 : lead >= 0xE0 ? 2 : lead >= 0xC0 ? 1 : 0;
		uint32_t codepoint = extra == 3 ? lead & 0x07u : extra == 2 ? lead & 0x0Fu : extra == 1 ? lead & 0x1Fu : 0xFFFDu;
		for (; extra > 0 && index < text.size(); extra--)
		{
			const uint8_t next = uint8_t(text[index]);
			if ((next & 0xC0) != 0x80)
				return 0xFFFD;
			codepoint = (codepoint << 6) | (next & 0x3Fu);
			index++;
		}
		return extra == 0 ? codepoint : 0xFFFD;
	}

	void UiDrawList::Reset(float scale)
	{
		m_Primitives.clear();
		m_Clips.clear();
		m_Scale = scale;
	}

	void UiDrawList::PushClip(const UiRect& rect)
	{
		m_Clips.push_back(m_Clips.empty() ? rect : m_Clips.back().Intersect(rect));
	}

	void UiDrawList::PopClip()
	{
		if (!m_Clips.empty())
			m_Clips.pop_back();
	}

	UiRect UiDrawList::GetClip() const
	{
		return m_Clips.empty() ? UiRect{ -1e6f, -1e6f, 2e6f, 2e6f } : m_Clips.back();
	}

	float UiDrawList::Snap(float value) const
	{
		return std::round(value * m_Scale) / m_Scale;
	}

	UiPrimitive& UiDrawList::Add(UiMode mode, glm::vec2 center, glm::vec2 halfSize)
	{
		UiPrimitive& primitive = m_Primitives.emplace_back();
		primitive.Rect = glm::vec4(center, halfSize);
		if (!m_Clips.empty())
		{
			const UiRect& clip = m_Clips.back();
			primitive.Clip = glm::vec4(clip.X, clip.Y, clip.Right(), clip.Bottom());
		}
		primitive.Info.x = uint32_t(mode);
		return primitive;
	}

	void UiDrawList::Rect(const UiRect& rect, const RectStyle& style)
	{
		if ((style.Fill.a <= 0.0f && Resolve(style.FillBottom, style.Fill).a <= 0.0f && style.BorderWidth <= 0.0f && style.Glow.a <= 0.0f) || rect.W <= 0.0f || rect.H <= 0.0f)
			return;
		UiPrimitive& primitive = Add(UiMode::Rect, rect.Center(), rect.Size() * 0.5f);
		primitive.Colors = glm::uvec4(PackColor(style.Fill), PackColor(Resolve(style.FillBottom, style.Fill)), PackColor(style.Border), PackColor(style.Glow));
		primitive.Shape = glm::vec4(style.Radius, style.BorderWidth, style.Softness, style.Rotation);
		primitive.Params = glm::vec4(style.Sweep, style.GlowSize, 0.0f, 0.0f);
		primitive.Info.z = style.Horizontal ? 1u : 0u;
	}

	void UiDrawList::Shadow(const UiRect& rect, float radius, float blur, UiColor color, glm::vec2 offset)
	{
		Rect(rect.Translate(offset), { .Fill = color, .Radius = radius, .Softness = blur });
	}

	void UiDrawList::Circle(glm::vec2 center, float radius, UiColor fill, UiColor border, float borderWidth)
	{
		Rect(UiRect::Centered(center, glm::vec2(radius * 2.0f)), { .Fill = fill, .Radius = radius, .Border = border, .BorderWidth = borderWidth });
	}

	void UiDrawList::Line(glm::vec2 from, glm::vec2 to, float width, UiColor color)
	{
		const glm::vec2 delta = to - from;
		const float length = glm::length(delta);
		if (length < 1e-4f || color.a <= 0.0f)
			return;
		UiPrimitive& primitive = Add(UiMode::Rect, (from + to) * 0.5f, glm::vec2(length * 0.5f + width * 0.5f, width * 0.5f));
		primitive.Colors = glm::uvec4(PackColor(color), PackColor(color), 0u, 0u);
		primitive.Shape = glm::vec4(width * 0.5f, 0.0f, 0.0f, std::atan2(delta.y, delta.x));
	}

	void UiDrawList::Triangle(glm::vec2 center, glm::vec2 size, float rotation, UiColor color, UiColor border, float borderWidth)
	{
		UiPrimitive& primitive = Add(UiMode::Triangle, center, size * 0.5f);
		primitive.Colors = glm::uvec4(PackColor(color), PackColor(color), PackColor(border), 0u);
		primitive.Shape = glm::vec4(0.0f, borderWidth, 0.0f, rotation);
	}

	void UiDrawList::Orb(glm::vec2 center, float radius, float fill, UiColor light, UiColor deep, UiColor glass, UiColor highlight, float phase, float wave)
	{
		UiPrimitive& primitive = Add(UiMode::Orb, center, glm::vec2(radius));
		primitive.Colors = glm::uvec4(PackColor(light), PackColor(deep), PackColor(glass), PackColor(highlight));
		primitive.Params = glm::vec4(fill, phase, wave, 0.0f);
	}

	void UiDrawList::Icon(UiIcon icon, const UiRect& rect, const IconStyle& style)
	{
		const AtlasIcon& entry = m_Assets->GetIcon(icon);
		if (!entry.Valid)
			return;
		const auto emit = [&](glm::vec2 offset, UiColor top, UiColor bottom, float outline, float softness)
		{
			const glm::vec2 min = rect.Min() + entry.PlaneMin * rect.Size() + offset;
			const glm::vec2 max = rect.Min() + entry.PlaneMax * rect.Size() + offset;
			UiPrimitive& primitive = Add(UiMode::Glyph, (min + max) * 0.5f, (max - min) * 0.5f);
			primitive.Uv = glm::vec4(entry.UvMin, entry.UvMax);
			primitive.Colors = glm::uvec4(PackColor(top), PackColor(bottom), PackColor(style.OutlineColor), 0u);
			primitive.Params = glm::vec4(style.Weight, outline, softness, 0.0f);
			primitive.Info.y = m_Assets->GetAtlasTexture();
		};
		if (style.ShadowColor.a > 0.0f)
			emit(style.ShadowOffset, style.ShadowColor, style.ShadowColor, 0.0f, std::max(style.ShadowBlur, 0.01f));
		emit(glm::vec2(0.0f), style.Color, Resolve(style.ColorBottom, style.Color), style.Outline, 0.0f);
	}

	void UiDrawList::Image(UiImage image, const UiRect& rect, UiColor tint, float saturation, float radius)
	{
		const UiImageRegion& region = m_Assets->GetImage(image);
		if (!region.Valid)
			return;
		UiPrimitive& primitive = Add(UiMode::Image, rect.Center(), rect.Size() * 0.5f);
		primitive.Uv = glm::vec4(region.UvMin, region.UvMax);
		primitive.Colors = glm::uvec4(PackColor(tint), PackColor(tint), 0u, 0u);
		primitive.Shape = glm::vec4(radius, 0.0f, 0.0f, 0.0f);
		primitive.Params = glm::vec4(saturation, 0.0f, 0.0f, 0.0f);
		primitive.Info.y = region.Texture;
	}

	float UiDrawList::LineHeight(const TextStyle& style) const
	{
		const FontFace& font = m_Assets->GetFont(style.Font);
		return (font.Ascender - font.Descender) * style.Size;
	}

	float UiDrawList::CapHeight(const TextStyle& style) const
	{
		return m_Assets->GetCapHeight(style.Font) * style.Size;
	}

	float UiDrawList::MeasureWidth(std::string_view text, const TextStyle& style) const
	{
		const FontFace& font = m_Assets->GetFont(style.Font);
		float width = 0.0f;
		uint32_t previous = 0;
		for (size_t index = 0; index < text.size();)
		{
			const uint32_t codepoint = DecodeUtf8(text, index);
			const AtlasGlyph* glyph = font.Find(codepoint);
			if (!glyph)
				continue;
			if (previous)
				width += font.GetKerning(previous, codepoint) * style.Size + style.Tracking;
			width += glyph->Advance * style.Size;
			previous = codepoint;
		}
		return width;
	}

	void UiDrawList::Glyphs(glm::vec2 pen, std::string_view text, const TextStyle& style, UiColor top, UiColor bottom, glm::vec2 offset, float outline, float softness)
	{
		const FontFace& font = m_Assets->GetFont(style.Font);
		const float lineTop = pen.y - CapHeight(style);
		const float lineHeight = std::max(pen.y - lineTop, 1.0f);
		const bool gradient = top != bottom;
		uint32_t previous = 0;
		for (size_t index = 0; index < text.size();)
		{
			const uint32_t codepoint = DecodeUtf8(text, index);
			const AtlasGlyph* glyph = font.Find(codepoint);
			if (!glyph)
				continue;
			if (previous)
				pen.x += font.GetKerning(previous, codepoint) * style.Size + style.Tracking;
			previous = codepoint;
			if (glyph->Visible)
			{
				const glm::vec2 min = pen + glyph->PlaneMin * style.Size + offset;
				const glm::vec2 max = pen + glyph->PlaneMax * style.Size + offset;
				UiPrimitive& primitive = Add(UiMode::Glyph, (min + max) * 0.5f, (max - min) * 0.5f);
				primitive.Uv = glm::vec4(glyph->UvMin, glyph->UvMax);
				UiColor glyphTop = top;
				UiColor glyphBottom = bottom;
				if (gradient)
				{
					glyphTop = glm::mix(top, bottom, (min.y - offset.y - lineTop) / lineHeight);
					glyphBottom = glm::mix(top, bottom, (max.y - offset.y - lineTop) / lineHeight);
				}
				primitive.Colors = glm::uvec4(PackColor(glyphTop), PackColor(glyphBottom), PackColor(style.OutlineColor), 0u);
				primitive.Params = glm::vec4(style.Weight, outline, softness, 0.0f);
				primitive.Info.y = m_Assets->GetAtlasTexture();
			}
			pen.x += glyph->Advance * style.Size;
		}
	}

	glm::vec2 UiDrawList::Text(glm::vec2 topLeft, std::string_view text, const TextStyle& style)
	{
		const FontFace& font = m_Assets->GetFont(style.Font);
		const glm::vec2 pen(Snap(topLeft.x), Snap(topLeft.y + font.Ascender * style.Size));
		if (style.ShadowColor.a > 0.0f)
			Glyphs(pen, text, style, style.ShadowColor, style.ShadowColor, style.ShadowOffset, 0.0f, std::max(style.ShadowBlur, 0.01f));
		if (style.Outline > 0.0f && style.OutlineColor.a > 0.0f)
			Glyphs(pen, text, style, UiColor(0.0f), UiColor(0.0f), glm::vec2(0.0f), style.Outline, 0.0f);
		Glyphs(pen, text, style, style.Color, Resolve(style.ColorBottom, style.Color), glm::vec2(0.0f), 0.0f, 0.0f);
		return { MeasureWidth(text, style), LineHeight(style) };
	}

	void UiDrawList::TextAligned(const UiRect& rect, std::string_view text, const TextStyle& style, glm::vec2 align)
	{
		const FontFace& font = m_Assets->GetFont(style.Font);
		const float width = MeasureWidth(text, style);
		const float baseline = glm::mix(rect.Y + CapHeight(style), rect.Bottom(), align.y);
		Text({ rect.X + (rect.W - width) * align.x, baseline - font.Ascender * style.Size }, text, style);
	}

	std::vector<TextLine> UiDrawList::WrapText(std::string_view text, const TextStyle& style, float width) const
	{
		std::vector<TextLine> lines;
		size_t lineStart = 0;
		while (lineStart <= text.size())
		{
			const size_t newline = text.find('\n', lineStart);
			const std::string_view paragraph = text.substr(lineStart, newline == std::string_view::npos ? std::string_view::npos : newline - lineStart);
			size_t start = 0;
			while (true)
			{
				size_t end = start;
				size_t lastBreak = std::string_view::npos;
				float lineWidth = 0.0f;
				float breakWidth = 0.0f;
				while (end < paragraph.size())
				{
					size_t next = end;
					const uint32_t codepoint = DecodeUtf8(paragraph, next);
					const float candidate = MeasureWidth(paragraph.substr(start, next - start), style);
					if (candidate > width && end > start)
						break;
					if (IsSpace(codepoint))
					{
						lastBreak = end;
						breakWidth = lineWidth;
					}
					lineWidth = candidate;
					end = next;
				}
				if (end >= paragraph.size())
				{
					lines.push_back({ paragraph.substr(start), MeasureWidth(paragraph.substr(start), style) });
					break;
				}
				const size_t cut = lastBreak != std::string_view::npos && lastBreak > start ? lastBreak : end;
				lines.push_back({ paragraph.substr(start, cut - start), cut == lastBreak ? breakWidth : lineWidth });
				start = cut;
				while (start < paragraph.size() && IsSpace(uint8_t(paragraph[start])))
					start++;
			}
			if (newline == std::string_view::npos)
				break;
			lineStart = newline + 1;
		}
		return lines;
	}

	float UiDrawList::TextWrapped(const UiRect& rect, std::string_view text, const TextStyle& style, float align, float lineSpacing)
	{
		const float lineHeight = LineHeight(style) * lineSpacing;
		float y = rect.Y;
		for (const TextLine& line : WrapText(text, style, rect.W))
		{
			Text({ rect.X + (rect.W - line.Width) * align, y }, line.Text, style);
			y += lineHeight;
		}
		return y - rect.Y;
	}

	std::string UiDrawList::Ellipsize(std::string_view text, const TextStyle& style, float width) const
	{
		if (MeasureWidth(text, style) <= width)
			return std::string(text);
		const bool hasEllipsis = m_Assets->GetFont(style.Font).Glyphs.contains(0x2026);
		const std::string ellipsis = hasEllipsis ? "\xE2\x80\xA6" : "...";
		const float available = width - MeasureWidth(ellipsis, style);
		size_t end = 0;
		for (size_t index = 0; index < text.size();)
		{
			size_t next = index;
			DecodeUtf8(text, next);
			if (MeasureWidth(text.substr(0, next), style) > available)
				break;
			end = index = next;
		}
		while (end > 0 && text[end - 1] == ' ')
			end--;
		return std::string(text.substr(0, end)) + ellipsis;
	}

	void UiDrawList::TextFit(const UiRect& rect, std::string_view text, const TextStyle& style, glm::vec2 align)
	{
		TextAligned(rect, Ellipsize(text, style, rect.W), style, align);
	}
}

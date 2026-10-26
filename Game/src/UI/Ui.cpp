#include "Ui.h"
#include <RadianceEngine/Window/Input.h>
#include <RadianceEngine/Window/Window.h>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr float DragThreshold = 5.0f;
		constexpr float RepeatDelay = 0.42f;
		constexpr float RepeatInterval = 0.035f;

		size_t PreviousBoundary(const std::string& text, size_t index)
		{
			if (index == 0)
				return 0;
			index--;
			while (index > 0 && (uint8_t(text[index]) & 0xC0) == 0x80)
				index--;
			return index;
		}

		size_t NextBoundary(const std::string& text, size_t index)
		{
			if (index >= text.size())
				return text.size();
			index++;
			while (index < text.size() && (uint8_t(text[index]) & 0xC0) == 0x80)
				index++;
			return index;
		}

		std::string EncodeUtf8(uint32_t codepoint)
		{
			std::string result;
			if (codepoint < 0x80)
				result += char(codepoint);
			else if (codepoint < 0x800)
			{
				result += char(0xC0 | (codepoint >> 6));
				result += char(0x80 | (codepoint & 0x3F));
			}
			else if (codepoint < 0x10000)
			{
				result += char(0xE0 | (codepoint >> 12));
				result += char(0x80 | ((codepoint >> 6) & 0x3F));
				result += char(0x80 | (codepoint & 0x3F));
			}
			else
			{
				result += char(0xF0 | (codepoint >> 18));
				result += char(0x80 | ((codepoint >> 12) & 0x3F));
				result += char(0x80 | ((codepoint >> 6) & 0x3F));
				result += char(0x80 | (codepoint & 0x3F));
			}
			return result;
		}

		void FadedLine(UiDrawList& draw, float x, float y, float width, UiColor color, float thickness = 1.0f)
		{
			const float half = width * 0.5f;
			draw.Rect({ x, y, half, thickness }, { .Fill = WithAlpha(color, 0.0f), .FillBottom = color, .Horizontal = true });
			draw.Rect({ x + half, y, width - half, thickness }, { .Fill = color, .FillBottom = WithAlpha(color, 0.0f), .Horizontal = true });
		}
	}

	namespace Theme
	{
		TextStyle Body(float size, UiColor color)
		{
			return { .Font = UiFont::Body, .Size = size, .Color = color };
		}

		TextStyle Bold(float size, UiColor color)
		{
			return { .Font = UiFont::Bold, .Size = size, .Color = color };
		}

		TextStyle Title(float size)
		{
			return { .Font = UiFont::Title, .Size = size, .Color = Hex(0xF7F4FF), .ColorBottom = Hex(0xB8A7EE), .Weight = 0.12f, .ShadowOffset = { 0.0f, 1.5f }, .ShadowColor = Rgba(0.0f, 0.0f, 0.02f, 0.85f), .ShadowBlur = 2.5f, .GlowColor = WithAlpha(Accent, 0.32f), .GlowBlur = 7.0f };
		}

		TextStyle Label(float size, UiColor color)
		{
			return { .Font = UiFont::Body, .Size = size, .Color = color };
		}

		TextStyle Floating(float size, UiColor color, UiFont font)
		{
			return { .Font = font, .Size = size, .Color = color, .Outline = 1.6f, .OutlineColor = Rgba(0.0f, 0.0f, 0.02f, 0.92f), .ShadowOffset = { 0.0f, 2.0f }, .ShadowColor = Rgba(0.0f, 0.0f, 0.02f, 0.55f), .ShadowBlur = 3.0f };
		}

		TextStyle Caps(float size, UiColor color, float tracking)
		{
			return { .Font = UiFont::Title, .Size = size, .Color = color, .Weight = 0.08f, .Tracking = tracking };
		}
	}

	UiTooltip& UiTooltip::Header(std::string text, UiColor color, std::optional<UiImage> image, std::optional<UiIcon> icon)
	{
		Line& line = Lines.emplace_back();
		line.Left = std::move(text);
		line.Style = Theme::Bold(18.0f, color);
		line.Image = image;
		line.Icon = icon;
		line.IconColor = color;
		return *this;
	}

	UiTooltip& UiTooltip::Text(std::string text, UiColor color, float size)
	{
		Line& line = Lines.emplace_back();
		line.Left = std::move(text);
		line.Style = Theme::Body(size, color);
		return *this;
	}

	UiTooltip& UiTooltip::Pair(std::string left, std::string right, UiColor leftColor, UiColor rightColor)
	{
		Line& line = Lines.emplace_back();
		line.Type = Kind::Pair;
		line.Left = std::move(left);
		line.Right = std::move(right);
		line.Style = Theme::Body(15.0f, leftColor);
		line.RightColor = rightColor;
		return *this;
	}

	UiTooltip& UiTooltip::Separator()
	{
		Lines.emplace_back().Type = Kind::Separator;
		return *this;
	}

	UiTooltip& UiTooltip::Space(float height)
	{
		Line& line = Lines.emplace_back();
		line.Type = Kind::Space;
		line.Height = height;
		return *this;
	}

	Ui::Ui(const UiAssets& assets, Rdn::Window& window)
		: m_Assets(assets), m_Window(window)
	{
		for (UiDrawList& list : m_Lists)
			list = UiDrawList(assets);
	}

	UiId Ui::MakeId(std::string_view name, uint64_t index)
	{
		uint64_t hash = 0xCBF29CE484222325ull;
		for (const char c : name)
			hash = (hash ^ uint8_t(c)) * 0x100000001B3ull;
		hash = (hash ^ index) * 0x100000001B3ull;
		return hash == 0 ? 1 : hash;
	}

	void Ui::BeginFrame(float dt, glm::vec2 framebufferSize, bool inputAllowed)
	{
		m_DeltaTime = dt;
		m_Motion = 0.0f;
		m_Time += dt;
		if (framebufferSize.x > 0.0f && framebufferSize.y > 0.0f)
			m_Scale = std::clamp(std::min(framebufferSize.x / 1600.0f, framebufferSize.y / 900.0f), 0.75f, 3.0f);
		m_Size = glm::max(framebufferSize, glm::vec2(1.0f)) / m_Scale;
		for (UiDrawList& list : m_Lists)
			list.Reset(m_Scale);
		m_Layer = UiLayer::Hud;
		m_PreviousMouse = m_Mouse;
		m_Mouse = glm::vec2(Rdn::Input::GetMouseX(), Rdn::Input::GetMouseY()) / m_Scale;
		m_InputAllowed = inputAllowed;
		m_LeftPressed = inputAllowed && Rdn::Input::IsKeyPressed(Rdn::KeyCode::MouseLeft);
		m_LeftReleased = Rdn::Input::IsKeyReleased(Rdn::KeyCode::MouseLeft);
		m_LeftDown = Rdn::Input::IsKeyDown(Rdn::KeyCode::MouseLeft);
		m_RightPressed = inputAllowed && Rdn::Input::IsKeyPressed(Rdn::KeyCode::MouseRight);
		m_Regions.clear();
		m_TooltipActive = false;
		m_DropAccepted = false;
		m_FocusClaimed = false;
		m_Edit.Blink += dt;
		m_EntryHoveredBefore = std::exchange(m_EntryHovered, 0);
		m_EntryCount = 0;
	}

	void Ui::EndFrame()
	{
		if (m_TooltipActive && !m_Drag)
			DrawTooltip();
		const Region* top = nullptr;
		for (const Region& region : m_Regions)
			if (region.Rect.Contains(m_Mouse) && (!top || region.Layer > top->Layer || (region.Layer == top->Layer && region.Order > top->Order)))
				top = &region;
		m_HoverId = top ? top->Id : 0;
		m_MouseOverUi = top != nullptr;
		if (m_LeftPressed && m_FocusId != 0 && !m_FocusClaimed)
			m_FocusId = 0;
		if (m_LeftReleased || !m_LeftDown)
		{
			m_ActiveId = 0;
			m_Drag.reset();
		}
	}

	void Ui::Collect(std::vector<UiPrimitive>& primitives) const
	{
		primitives.clear();
		for (const UiDrawList& list : m_Lists)
			primitives.insert(primitives.end(), list.GetPrimitives().begin(), list.GetPrimitives().end());
	}

	void Ui::Register(UiId id, const UiRect& rect)
	{
		m_Regions.push_back({ id, rect.Intersect(Draw().GetClip()), m_Layer, uint32_t(m_Regions.size()) });
	}

	UiInteraction Ui::Interact(UiId id, const UiRect& rect)
	{
		Register(id, rect);
		UiInteraction result;
		result.Hovered = m_InputAllowed && m_HoverId == id;
		if (result.Hovered && m_LeftPressed)
		{
			m_ActiveId = id;
			m_PressPosition = m_Mouse;
			result.Pressed = true;
		}
		if (m_ActiveId == id)
		{
			result.Held = m_LeftDown || result.Pressed;
			if (m_LeftReleased)
				result.Clicked = result.Hovered && !(m_Drag && m_Drag->Source == id);
		}
		result.RightClicked = result.Hovered && m_RightPressed;
		return result;
	}

	void Ui::Block(const UiRect& rect)
	{
		Register(0, rect);
	}

	bool Ui::WantsMouse() const
	{
		return m_InputAllowed && (m_MouseOverUi || m_ActiveId != 0 || m_Drag.has_value());
	}

	float Ui::Animate(UiId id, bool target, float speed)
	{
		float& value = m_Animations[id];
		const float goal = target ? 1.0f : 0.0f;
		const float step = std::min(m_DeltaTime * speed, 1.0f);
		value += (goal - value) * step;
		if (std::abs(goal - value) < 0.002f)
			value = goal;
		return value;
	}

	float Ui::Since(UiId id, bool active)
	{
		if (!active)
		{
			m_Timers.erase(id);
			return 0.0f;
		}
		return m_Timers[id] += m_DeltaTime;
	}

	void Ui::OfferDrag(UiId id, int type, int value)
	{
		if (m_ActiveId == id && m_LeftDown && !m_Drag && glm::length(m_Mouse - m_PressPosition) > DragThreshold)
			m_Drag = UiPayload{ type, value, id };
	}

	std::optional<int> Ui::AcceptDrop(const UiRect& rect, int type)
	{
		if (!m_Drag || m_Drag->Type != type || !m_LeftReleased || m_DropAccepted || !rect.Intersect(Draw().GetClip()).Contains(m_Mouse))
			return std::nullopt;
		m_DropAccepted = true;
		return m_Drag->Value;
	}

	void Ui::Panel(const UiRect& rect, float chamfer, bool blocks)
	{
		UiDrawList& draw = Draw();
		const float seed = std::fmod(std::abs(rect.X * 0.0131f + rect.Y * 0.0077f + rect.W * 0.0019f), 1.0f);
		draw.Rect(rect.Translate({ 0.0f, 10.0f }).Expand(4.0f), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.6f), .Softness = 24.0f, .Chamfer = chamfer + 4.0f });
		draw.Rect(rect, { .Fill = Theme::PanelTop, .FillBottom = Theme::PanelBottom, .Border = WithAlpha(Theme::Edge, 0.95f), .BorderWidth = 1.2f, .Glow = WithAlpha(Theme::Accent, 0.06f), .GlowSize = 48.0f, .Chamfer = chamfer, .Flow = 0.75f * m_Motion, .Aether = 1.0f, .Seed = seed, .Stars = m_Motion });
		draw.Rect(rect.Shrink(5.0f), { .Border = WithAlpha(Theme::AccentLight, 0.07f), .BorderWidth = 1.0f, .Chamfer = std::max(chamfer - 2.0f, 0.0f) });
		FadedLine(draw, rect.X + chamfer + 8.0f, rect.Y + 0.5f, rect.W - 2.0f * chamfer - 16.0f, WithAlpha(Theme::AccentLight, 0.35f));
		if (chamfer >= 6.0f)
		{
			const float half = chamfer * 0.5f;
			for (const glm::vec2 corner : { glm::vec2(rect.X + half, rect.Y + half), glm::vec2(rect.Right() - half, rect.Y + half), glm::vec2(rect.X + half, rect.Bottom() - half), glm::vec2(rect.Right() - half, rect.Bottom() - half) })
			{
				draw.Rect(UiRect::Centered(corner, glm::vec2(12.0f)), { .Fill = WithAlpha(Theme::Accent, 0.5f), .Radius = 6.0f, .Softness = 5.0f, .Additive = 1.0f });
				draw.Diamond(corner, 9.0f, Theme::AccentLight, WithAlpha(Theme::Ink, 0.9f), 1.2f);
			}
		}
		if (blocks)
			Block(rect);
	}

	void Ui::Card(const UiRect& rect, float hover, float selected, UiColor accent)
	{
		UiDrawList& draw = Draw();
		const float seed = std::fmod(std::abs(rect.X * 0.0173f + rect.Y * 0.0091f), 1.0f);
		if (selected > 0.01f)
			draw.Rect(rect.Expand(2.0f), { .Fill = WithAlpha(accent, 0.2f * selected), .Softness = 12.0f, .Chamfer = 10.0f, .Additive = 1.0f });
		draw.Rect(rect, { .Fill = glm::mix(Theme::CardTop, Hex(0x251C4A, 0.94f), selected), .FillBottom = glm::mix(Theme::CardBottom, Hex(0x110C27, 0.95f), selected), .Border = glm::mix(WithAlpha(Theme::Edge, 0.5f + 0.4f * hover), accent, selected),
			.BorderWidth = 1.2f + 0.5f * selected, .Glow = WithAlpha(glm::mix(Theme::Astral, accent, selected), 0.05f * hover + 0.1f * selected), .GlowSize = 18.0f, .Chamfer = 8.0f, .Flow = 0.8f * selected * m_Motion, .Aether = 0.25f + 0.5f * selected, .Seed = seed });
	}

	UiRect Ui::Window(UiId id, std::string_view title, UiRect rect, bool* open)
	{
		WindowState& state = m_Windows[id];
		const glm::vec2 home = rect.Min();
		rect = rect.Translate(state.Offset);
		rect.X = std::clamp(rect.X, 0.0f, std::max(m_Size.x - rect.W, 0.0f));
		rect.Y = std::clamp(rect.Y, 0.0f, std::max(m_Size.y - rect.H, 0.0f));
		state.Offset = rect.Min() - home;
		Panel(rect, 16.0f, true);

		UiDrawList& draw = Draw();
		const UiRect header{ rect.X, rect.Y, rect.W, 46.0f };
		const UiId dragId = id ^ 0x5DEECE66Dull;
		if (Interact(dragId, header).Held && m_ActiveId == dragId)
			state.Offset += m_Mouse - m_PreviousMouse;
		draw.Rect(header.Shrink(3.0f, 3.0f), { .Fill = WithAlpha(Theme::Accent, 0.1f), .FillBottom = WithAlpha(Theme::Accent, 0.0f), .Chamfer = 13.0f });
		draw.Sigil({ rect.X + 34.0f, header.Center().y }, 14.0f, { .Color = WithAlpha(Theme::AccentLight, 0.85f), .Accent = WithAlpha(Theme::Astral, 0.9f), .Speed = 0.25f, .Phase = float(id % 1000) / 1000.0f, .LineWidth = 1.0f, .Glow = 0.45f, .Layers = SigilRings | SigilStar, .Points = 5, .Skip = 2 });
		TextStyle titleStyle = Theme::Title(21.0f);
		titleStyle.Tracking = 1.5f;
		draw.TextAligned({ rect.X + 58.0f, header.Y, header.W - 110.0f, header.H }, title, titleStyle, { 0.0f, 0.5f });
		FadedLine(draw, rect.X + 16.0f, header.Bottom() - 1.0f, rect.W - 32.0f, WithAlpha(Theme::Edge, 0.95f));
		draw.Diamond({ rect.Center().x, header.Bottom() - 0.5f }, 7.0f, Theme::AccentLight, WithAlpha(Theme::Ink, 0.9f), 1.0f);
		if (open)
		{
			const UiRect close{ rect.Right() - 44.0f, rect.Y + 9.0f, 28.0f, 28.0f };
			const UiId closeId = id ^ 0xB5AD4ECEDA1CE2A9ull;
			const UiInteraction interaction = Interact(closeId, close);
			const float hover = Animate(closeId, interaction.Hovered);
			const glm::vec2 center = close.Center();
			draw.Diamond(center, 26.0f, Rgba(0.94f, 0.39f, 0.43f, 0.16f * hover), WithAlpha(glm::mix(Theme::Edge, Theme::Danger, hover), 0.6f + 0.4f * hover), 1.0f);
			const UiColor cross = glm::mix(Theme::Muted, Hex(0xFFB8C0), hover);
			draw.Line(center - glm::vec2(4.5f), center + glm::vec2(4.5f), 1.6f, cross);
			draw.Line(center + glm::vec2(-4.5f, 4.5f), center + glm::vec2(4.5f, -4.5f), 1.6f, cross);
			if (interaction.Clicked)
				*open = false;
		}
		return { rect.X + 18.0f, header.Bottom() + 10.0f, rect.W - 36.0f, rect.H - header.H - 26.0f };
	}

	void Ui::Separator(const UiRect& rect, std::string_view label)
	{
		UiDrawList& draw = Draw();
		const float y = std::round(rect.Center().y);
		if (label.empty())
		{
			FadedLine(draw, rect.X, y, rect.W, WithAlpha(Theme::Edge, 0.95f));
			draw.Diamond({ rect.Center().x, y + 0.5f }, 7.0f, Theme::AccentLight, WithAlpha(Theme::Ink, 0.9f), 1.0f);
			return;
		}
		const TextStyle style = Theme::Caps(13.5f, Theme::AccentLight, 1.6f);
		draw.Diamond({ rect.X + 4.0f, y + 0.5f }, 7.0f, Theme::Accent);
		const float width = draw.MeasureWidth(label, style);
		draw.TextAligned({ rect.X + 14.0f, rect.Y, width, rect.H }, label, style, { 0.0f, 0.5f });
		const float start = rect.X + width + 24.0f;
		if (rect.Right() > start)
			draw.Rect({ start, y, rect.Right() - start, 1.0f }, { .Fill = WithAlpha(Theme::Edge, 0.95f), .FillBottom = WithAlpha(Theme::Edge, 0.0f), .Horizontal = true });
	}

	bool Ui::Button(UiId id, const UiRect& rect, std::string_view label, ButtonKind kind, bool enabled, std::optional<UiIcon> icon)
	{
		UiInteraction interaction;
		if (enabled)
			interaction = Interact(id, rect);
		else
			Block(rect);
		const float hover = Animate(id, interaction.Hovered);
		const bool pressed = interaction.Held && interaction.Hovered;
		UiColor top = Hex(0x1D1836, 0.92f);
		UiColor bottom = Hex(0x0D0B1B, 0.94f);
		UiColor border = Theme::Edge;
		UiColor light = Theme::Astral;
		UiColor text = Theme::Text;
		if (kind == ButtonKind::Primary)
		{
			top = Hex(0x3C2C74);
			bottom = Hex(0x1A1241);
			border = Theme::Gold;
			light = Theme::Gold;
			text = Theme::GoldLight;
		}
		else if (kind == ButtonKind::Danger)
		{
			top = Hex(0x4C1831);
			bottom = Hex(0x250A17);
			border = Hex(0xD9607E);
			light = Hex(0xFF8FA6);
			text = Hex(0xFFE1E7);
		}
		top = Lighten(top, 0.1f * hover);
		bottom = Lighten(bottom, 0.05f * hover);
		border = glm::mix(border, Lighten(light, 0.3f), 0.55f * hover);
		if (pressed)
		{
			top = Darken(top, 0.2f);
			bottom = Darken(bottom, 0.1f);
		}
		if (!enabled)
		{
			top = WithAlpha(Darken(top, 0.35f), 0.8f);
			bottom = WithAlpha(Darken(bottom, 0.35f), 0.8f);
			border = WithAlpha(border, 0.3f);
			text = Theme::Faint;
		}
		UiDrawList& draw = Draw();
		const UiRect body = rect.Translate({ 0.0f, pressed ? 1.0f : 0.0f });
		const float chamfer = std::min(8.0f, body.H * 0.3f);
		const float seed = float(id % 997u) / 997.0f;
		draw.Rect(rect.Translate({ 0.0f, 3.0f }), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.5f), .Softness = 6.0f, .Chamfer = chamfer });
		if (enabled && hover > 0.01f)
			draw.Rect(body.Expand(3.0f), { .Fill = WithAlpha(light, 0.16f * hover), .Softness = 10.0f, .Chamfer = chamfer + 3.0f, .Additive = 1.0f });
		const float flow = enabled ? ((kind == ButtonKind::Primary ? 0.55f : 0.0f) + 0.6f * hover) * m_Motion : 0.0f;
		draw.Rect(body, { .Fill = top, .FillBottom = bottom, .Border = border, .BorderWidth = 1.2f, .Glow = WithAlpha(light, enabled ? 0.07f + 0.15f * hover : 0.0f), .GlowSize = 12.0f, .Chamfer = chamfer, .Flow = flow, .Seed = seed });
		if (enabled && hover > 0.01f)
		{
			const float cycle = std::fmod(Since(id ^ 0x9E3779B97F4A7C15ull, interaction.Hovered), 2.6f) / 0.9f;
			if (cycle < 1.0f)
			{
				const float band = std::max(body.H * 1.2f, 30.0f);
				const float x = body.X - band + cycle * (body.W + 2.0f * band);
				draw.PushClip(body.Shrink(1.5f));
				draw.Rect({ x, body.Y, band * 0.5f, body.H }, { .Fill = WithAlpha(light, 0.0f), .FillBottom = WithAlpha(light, 0.2f * hover), .Horizontal = true, .Additive = 1.0f });
				draw.Rect({ x + band * 0.5f, body.Y, band * 0.5f, body.H }, { .Fill = WithAlpha(light, 0.2f * hover), .FillBottom = WithAlpha(light, 0.0f), .Horizontal = true, .Additive = 1.0f });
				draw.PopClip();
			}
		}
		FadedLine(draw, body.X + chamfer, body.Y + 1.0f, body.W - 2.0f * chamfer, Rgba(0.9f, 0.88f, 1.0f, 0.08f + 0.1f * hover));
		const float iconSize = icon ? 19.0f : 0.0f;
		const float gap = icon && !label.empty() ? 8.0f : 0.0f;
		TextStyle style = Theme::Caps(15.5f, text, 0.9f);
		float textWidth = draw.MeasureWidth(label, style);
		const float available = body.W - 20.0f - iconSize - gap;
		if (textWidth > available && textWidth > 0.0f)
		{
			style.Size *= std::max(available / textWidth, 0.7f);
			textWidth = draw.MeasureWidth(label, style);
		}
		style.ShadowOffset = { 0.0f, 1.0f };
		style.ShadowColor = Rgba(0.0f, 0.0f, 0.02f, 0.7f);
		style.ShadowBlur = 1.5f;
		style.GlowColor = WithAlpha(light, enabled ? 0.12f + 0.25f * hover : 0.0f);
		style.GlowBlur = 5.0f;
		const float start = body.Center().x - (iconSize + gap + textWidth) * 0.5f;
		if (icon)
			draw.Icon(*icon, UiRect::Centered({ start + iconSize * 0.5f, body.Center().y }, glm::vec2(iconSize)), { .Color = text, .ShadowColor = Rgba(0.0f, 0.0f, 0.02f, 0.6f), .GlowColor = WithAlpha(light, enabled ? 0.2f * hover : 0.0f) });
		draw.TextAligned({ start + iconSize + gap, body.Y, textWidth + 2.0f, body.H }, label, style, { 0.0f, 0.5f });
		const bool clicked = enabled && interaction.Clicked;
		m_Clicks += clicked ? 1u : 0u;
		return clicked;
	}

	bool Ui::MenuEntry(UiId id, const UiRect& rect, std::string_view title, std::string_view subtitle, bool selected, bool centered)
	{
		const UiInteraction interaction = Interact(id, rect);
		if (interaction.Hovered)
		{
			if (m_EntryHoveredBefore != id)
				m_HoverEntry = m_EntryCount;
			m_EntryHovered = id;
		}
		m_EntryCount++;
		const float hover = Animate(id, interaction.Hovered, 9.0f);
		const float chosen = Animate(id ^ 0xA5A5A5A55A5A5A5Aull, selected, 8.0f);
		const float lit = std::max(hover, chosen);
		const UiColor tone = selected ? Theme::Gold : Theme::Accent;
		UiDrawList& draw = Draw();
		TextStyle titleStyle = Theme::Caps(25.0f, glm::mix(Theme::Text, selected ? Theme::GoldLight : Hex(0xFFFFFF), lit), 2.0f + 0.8f * hover);
		titleStyle.ColorBottom = glm::mix(Theme::Muted, selected ? Theme::Gold : Theme::AccentLight, lit);
		titleStyle.ShadowOffset = { 0.0f, 2.0f };
		titleStyle.ShadowColor = Rgba(0.0f, 0.0f, 0.02f, 0.8f);
		titleStyle.ShadowBlur = 3.0f;
		titleStyle.GlowColor = WithAlpha(tone, 0.5f * lit);
		titleStyle.GlowBlur = 9.0f;
		titleStyle.Shimmer = 0.45f * hover;
		const TextStyle subtitleStyle = Theme::Body(14.5f, glm::mix(Theme::Faint, Theme::Muted, lit));
		const float titleWidth = draw.MeasureWidth(title, titleStyle);
		const float top = subtitle.empty() ? rect.Center().y - 16.0f : rect.Y + 2.0f;
		const float middle = top + 16.0f;
		const float x = centered ? rect.Center().x - titleWidth * 0.5f : rect.X + 34.0f + 8.0f * lit;
		draw.TextAligned({ x, top, titleWidth + 4.0f, 32.0f }, title, titleStyle, { 0.0f, 0.5f });
		if (!subtitle.empty())
		{
			const float subtitleWidth = draw.MeasureWidth(subtitle, subtitleStyle);
			draw.Text({ centered ? rect.Center().x - subtitleWidth * 0.5f : x + 2.0f, top + 33.0f }, subtitle, subtitleStyle);
		}
		const auto marker = [&](glm::vec2 at)
		{
			if (lit > 0.01f)
				draw.Rect(UiRect::Centered(at, glm::vec2(18.0f)), { .Fill = WithAlpha(tone, 0.45f * lit), .Radius = 9.0f, .Softness = 7.0f, .Additive = 1.0f });
			draw.Diamond(at, 10.0f + 3.0f * lit, glm::mix(WithAlpha(Theme::Ink, 0.6f), selected ? Theme::Gold : Theme::AccentLight, lit), glm::mix(Theme::Edge, Theme::AccentLight, lit), 1.2f);
			if (chosen > 0.01f)
				draw.Sigil(at, 15.0f, { .Color = WithAlpha(Theme::Gold, 0.85f * chosen), .Accent = WithAlpha(Theme::AccentLight, 0.85f * chosen), .Speed = 2.0f, .LineWidth = 1.0f, .Glow = 0.5f, .Layers = SigilRings, .Progress = chosen });
		};
		if (!centered)
			marker({ rect.X + 14.0f, middle });
		else
		{
			marker({ x - 24.0f, middle });
			marker({ x + titleWidth + 24.0f, middle });
		}
		if (lit > 0.01f)
		{
			const float width = (titleWidth + 30.0f) * lit;
			if (!centered)
				draw.Rect({ x - 6.0f, top + 30.0f, width, 1.0f }, { .Fill = WithAlpha(Theme::AccentLight, 0.75f * lit), .FillBottom = WithAlpha(Theme::AccentLight, 0.0f), .Horizontal = true });
			else
				FadedLine(draw, rect.Center().x - width * 0.5f, top + 30.0f, width, WithAlpha(Theme::AccentLight, 0.75f * lit));
		}
		const bool clicked = interaction.Clicked;
		m_Clicks += clicked ? 1u : 0u;
		return clicked;
	}

	bool Ui::Checkbox(UiId id, const UiRect& rect, std::string_view label, bool& value)
	{
		const UiInteraction interaction = Interact(id, rect);
		const float hover = Animate(id, interaction.Hovered);
		const float on = Animate(id ^ 0xC0FFEE1234567ull, value, 14.0f);
		UiDrawList& draw = Draw();
		const glm::vec2 center(rect.X + 10.0f, rect.Center().y);
		if (on > 0.01f)
			draw.Rect(UiRect::Centered(center, glm::vec2(22.0f)), { .Fill = WithAlpha(Theme::Accent, 0.35f * on), .Radius = 11.0f, .Softness = 8.0f, .Additive = 1.0f });
		draw.Diamond(center, 19.0f, Theme::SlotBottom, glm::mix(Theme::Edge, Theme::Astral, hover), 1.3f);
		if (on > 0.01f)
			draw.Diamond(center, 10.0f * on, glm::mix(Theme::Accent, Theme::AccentLight, 0.45f));
		draw.TextAligned({ rect.X + 28.0f, rect.Y, rect.W - 28.0f, rect.H }, label, Theme::Body(16.0f, glm::mix(Theme::Text, Theme::AccentLight, hover * 0.7f)), { 0.0f, 0.5f });
		if (interaction.Clicked)
		{
			value = !value;
			m_Clicks++;
			return true;
		}
		return false;
	}

	bool Ui::Slider(UiId id, const UiRect& rect, float& value, float min, float max, std::string_view text)
	{
		const UiInteraction interaction = Interact(id, rect);
		const float hover = Animate(id, interaction.Hovered || interaction.Held);
		UiRect area = rect;
		const UiRect label = area.CutRight(64.0f);
		const UiRect track{ area.X + 8.0f, area.Center().y - 1.0f, area.W - 16.0f, 2.0f };
		bool changed = false;
		if (interaction.Held && max > min)
		{
			const float next = min + std::clamp((m_Mouse.x - track.X) / std::max(track.W, 1.0f), 0.0f, 1.0f) * (max - min);
			changed = next != value;
			value = next;
		}
		const float fraction = max > min ? std::clamp((value - min) / (max - min), 0.0f, 1.0f) : 0.0f;
		UiDrawList& draw = Draw();
		draw.Rect(track, { .Fill = WithAlpha(Theme::Edge, 0.45f) });
		for (int i = 0; i <= 10; i++)
		{
			const float x = track.X + track.W * float(i) / 10.0f;
			const float height = i % 5 == 0 ? 9.0f : 5.0f;
			const bool lit = float(i) / 10.0f <= fraction + 1e-4f;
			draw.Rect({ x - 0.5f, track.Center().y - height * 0.5f, 1.0f, height }, { .Fill = lit ? WithAlpha(Theme::Accent, 0.85f) : WithAlpha(Theme::Edge, 0.55f) });
		}
		draw.Rect({ track.X, track.Y - 0.5f, track.W * fraction, track.H + 1.0f }, { .Fill = Theme::AccentDark, .FillBottom = Theme::AccentLight, .Horizontal = true });
		draw.Rect({ track.X, track.Center().y - 4.0f, track.W * fraction, 8.0f }, { .Fill = WithAlpha(Theme::Accent, 0.0f), .FillBottom = WithAlpha(Theme::Accent, 0.35f), .Softness = 3.0f, .Horizontal = true, .Additive = 1.0f });
		const glm::vec2 knob(track.X + track.W * fraction, track.Center().y);
		draw.Rect(UiRect::Centered(knob, glm::vec2(22.0f)), { .Fill = WithAlpha(Theme::Astral, 0.22f + 0.3f * hover), .Radius = 11.0f, .Softness = 7.0f, .Additive = 1.0f });
		draw.Diamond(knob, 15.0f + 2.0f * hover, glm::mix(Theme::AccentLight, Theme::Astral, hover * 0.5f), Theme::Ink, 1.2f);
		draw.TextAligned(label, text, Theme::Body(15.0f, Theme::Muted), { 1.0f, 0.5f });
		return changed;
	}

	bool Ui::Choice(UiId id, const UiRect& rect, int& index, std::span<const char* const> options)
	{
		const int count = int(options.size());
		if (count == 0)
			return false;
		index = std::clamp(index, 0, count - 1);
		UiDrawList& draw = Draw();
		UiRect area = rect;
		const UiRect left = area.CutLeft(26.0f);
		const UiRect right = area.CutRight(26.0f);
		const UiInteraction previous = Interact(id ^ 0x1F1F1F1F1F1Full, left);
		const UiInteraction next = Interact(id ^ 0x2E2E2E2E2E2Eull, right);
		const UiInteraction middle = Interact(id, area);
		const float hover = Animate(id, middle.Hovered);
		draw.Rect(area.Shrink(2.0f, 2.0f), { .Fill = Hex(0x0A0816, 0.8f), .FillBottom = Hex(0x15102B, 0.8f), .Border = WithAlpha(glm::mix(Theme::Edge, Theme::Astral, hover), 0.75f), .BorderWidth = 1.0f, .Chamfer = 6.0f });
		const float leftHover = Animate(id ^ 0x1F1F1F1F1F1Full, previous.Hovered);
		const float rightHover = Animate(id ^ 0x2E2E2E2E2E2Eull, next.Hovered);
		draw.Triangle(left.Center(), { 10.0f, 9.0f }, -Pi * 0.5f, glm::mix(Theme::Muted, Theme::Astral, leftHover));
		draw.Triangle(right.Center(), { 10.0f, 9.0f }, Pi * 0.5f, glm::mix(Theme::Muted, Theme::Astral, rightHover));
		draw.TextAligned({ area.X, area.Y - 3.0f, area.W, area.H }, options[size_t(index)], Theme::Bold(15.5f, glm::mix(Theme::Text, Theme::AccentLight, hover)), { 0.5f, 0.5f });
		const float pipY = area.Bottom() - 7.0f;
		for (int i = 0; i < count; i++)
		{
			const glm::vec2 pip(area.Center().x + (float(i) - float(count - 1) * 0.5f) * 10.0f, pipY);
			draw.Diamond(pip, 5.0f, i == index ? Theme::Accent : WithAlpha(Theme::Edge, 0.6f));
		}
		int step = 0;
		if (previous.Clicked)
			step = -1;
		if (next.Clicked || middle.Clicked)
			step = 1;
		if (step == 0)
			return false;
		index = (index + step + count) % count;
		m_Clicks++;
		return true;
	}

	bool Ui::Tabs(UiId id, const UiRect& rect, int& index, std::span<const char* const> labels)
	{
		const int count = int(labels.size());
		if (count == 0)
			return false;
		index = std::clamp(index, 0, count - 1);
		UiDrawList& draw = Draw();
		const float line = std::round(rect.Bottom()) - 1.0f;
		FadedLine(draw, rect.X, line, rect.W, WithAlpha(Theme::Edge, 0.95f));
		const float width = rect.W / float(count);
		bool changed = false;
		for (int i = 0; i < count; i++)
		{
			const UiRect tab{ rect.X + float(i) * width, rect.Y, width, rect.H };
			const UiId tabId = id + uint64_t(i) + 1;
			const UiInteraction interaction = Interact(tabId, tab);
			const float hover = Animate(tabId, interaction.Hovered);
			const float on = Animate(tabId ^ 0x5A17ED7AB5ull, i == index, 14.0f);
			if (on > 0.01f)
			{
				draw.Rect({ tab.X + 8.0f, tab.Y + 4.0f, tab.W - 16.0f, line - tab.Y - 4.0f }, { .Fill = WithAlpha(Theme::Accent, 0.0f), .FillBottom = WithAlpha(Theme::Accent, 0.16f * on), .Chamfer = 6.0f });
				const float underline = (tab.W - 28.0f) * (0.4f + 0.6f * on);
				draw.Rect({ tab.Center().x - underline * 0.5f, line - 1.0f, underline, 3.0f }, { .Fill = WithAlpha(Theme::GoldLight, on), .FillBottom = WithAlpha(Theme::Gold, on), .Glow = WithAlpha(Theme::Gold, 0.35f * on), .GlowSize = 8.0f });
				draw.Diamond({ tab.Center().x, line + 0.5f }, 8.0f * on, WithAlpha(Theme::GoldLight, on), WithAlpha(Theme::Ink, 0.9f * on), 1.0f);
			}
			else if (hover > 0.01f)
				draw.Rect({ tab.X + 8.0f, tab.Y + 4.0f, tab.W - 16.0f, line - tab.Y - 4.0f }, { .Fill = WithAlpha(Theme::Astral, 0.0f), .FillBottom = WithAlpha(Theme::Astral, 0.08f * hover), .Chamfer = 6.0f });
			const UiColor color = glm::mix(glm::mix(Theme::Muted, Theme::AccentLight, hover), Theme::GoldLight, on);
			draw.TextAligned({ tab.X, tab.Y, tab.W, line - tab.Y }, labels[size_t(i)], Theme::Caps(14.5f, color, 2.0f), { 0.5f, 0.5f });
			if (interaction.Clicked && i != index)
			{
				index = i;
				m_Clicks++;
				changed = true;
			}
		}
		return changed;
	}

	bool Ui::Repeat(Rdn::KeyCode key)
	{
		if (Rdn::Input::IsKeyPressed(key))
		{
			m_RepeatTimers[int(key)] = RepeatDelay;
			return true;
		}
		if (!Rdn::Input::IsKeyDown(key))
		{
			m_RepeatTimers.erase(int(key));
			return false;
		}
		float& timer = m_RepeatTimers[int(key)];
		timer -= m_DeltaTime;
		if (timer > 0.0f)
			return false;
		timer += RepeatInterval;
		return true;
	}

	void Ui::EditText(std::string& text, size_t maxLength, bool (*accept)(uint32_t), bool& changed)
	{
		using Rdn::Input;
		using Rdn::KeyCode;
		const bool control = Input::IsKeyDown(KeyCode::LeftControl) || Input::IsKeyDown(KeyCode::RightControl);
		const bool shift = Input::IsKeyDown(KeyCode::LeftShift) || Input::IsKeyDown(KeyCode::RightShift);
		size_t& caret = m_Edit.Caret;
		size_t& anchor = m_Edit.Anchor;
		caret = std::min(caret, text.size());
		anchor = std::min(anchor, text.size());
		bool touched = false;
		const auto eraseSelection = [&]
		{
			if (caret == anchor)
				return false;
			const size_t from = std::min(caret, anchor);
			text.erase(from, std::max(caret, anchor) - from);
			caret = anchor = from;
			changed = true;
			return true;
		};
		const auto insert = [&](std::string_view input)
		{
			for (size_t index = 0; index < input.size();)
			{
				const uint32_t codepoint = DecodeUtf8(input, index);
				if (codepoint == '\n' || codepoint == '\r')
					break;
				if (codepoint < 32 || codepoint == 127 || (accept && !accept(codepoint)))
					continue;
				eraseSelection();
				const std::string bytes = EncodeUtf8(codepoint);
				if (text.size() + bytes.size() > maxLength)
					break;
				text.insert(caret, bytes);
				caret += bytes.size();
				anchor = caret;
				changed = true;
			}
		};

		for (const uint32_t codepoint : Input::GetTypedCharacters())
		{
			insert(EncodeUtf8(codepoint));
			touched = true;
		}
		if (Repeat(KeyCode::Backspace))
		{
			if (!eraseSelection() && caret > 0)
			{
				const size_t from = PreviousBoundary(text, caret);
				text.erase(from, caret - from);
				caret = anchor = from;
				changed = true;
			}
			touched = true;
		}
		if (Repeat(KeyCode::Delete))
		{
			if (!eraseSelection() && caret < text.size())
			{
				text.erase(caret, NextBoundary(text, caret) - caret);
				anchor = caret;
				changed = true;
			}
			touched = true;
		}
		const auto move = [&](size_t target)
		{
			caret = target;
			if (!shift)
				anchor = caret;
			touched = true;
		};
		if (Repeat(KeyCode::Left))
			move(!shift && caret != anchor ? std::min(caret, anchor) : PreviousBoundary(text, caret));
		if (Repeat(KeyCode::Right))
			move(!shift && caret != anchor ? std::max(caret, anchor) : NextBoundary(text, caret));
		if (Input::IsKeyPressed(KeyCode::Home))
			move(0);
		if (Input::IsKeyPressed(KeyCode::End))
			move(text.size());
		if (control && Input::IsKeyPressed(KeyCode::A))
		{
			anchor = 0;
			caret = text.size();
			touched = true;
		}
		if (control && (Input::IsKeyPressed(KeyCode::C) || Input::IsKeyPressed(KeyCode::X)) && caret != anchor)
		{
			const size_t from = std::min(caret, anchor);
			m_Window.SetClipboardText(text.substr(from, std::max(caret, anchor) - from));
			if (Input::IsKeyPressed(KeyCode::X))
				eraseSelection();
			touched = true;
		}
		if (control && Input::IsKeyPressed(KeyCode::V))
		{
			insert(m_Window.GetClipboardText());
			touched = true;
		}
		if (Input::IsKeyPressed(KeyCode::Enter) || Input::IsKeyPressed(KeyCode::KpEnter) || Input::IsKeyPressed(KeyCode::Escape) || Input::IsKeyPressed(KeyCode::Tab))
			m_FocusId = 0;
		if (touched)
			m_Edit.Blink = 0.0f;
	}

	bool Ui::TextField(UiId id, const UiRect& rect, std::string& text, size_t maxLength, std::string_view placeholder, bool (*accept)(uint32_t))
	{
		const UiInteraction interaction = Interact(id, rect);
		const TextStyle style = Theme::Body(17.0f);
		const UiRect area = rect.Shrink(11.0f, 0.0f);
		UiDrawList& draw = Draw();
		const auto caretFromMouse = [&]
		{
			const float target = m_Mouse.x - area.X + m_Edit.Scroll;
			size_t best = 0;
			float bestDistance = std::abs(target);
			for (size_t index = 0; index < text.size();)
			{
				index = NextBoundary(text, index);
				const float distance = std::abs(draw.MeasureWidth(std::string_view(text).substr(0, index), style) - target);
				if (distance < bestDistance)
				{
					bestDistance = distance;
					best = index;
				}
			}
			return best;
		};
		if (interaction.Pressed)
		{
			if (m_FocusId != id)
			{
				m_FocusId = id;
				m_Edit = {};
			}
			m_Edit.Caret = m_Edit.Anchor = caretFromMouse();
			m_Edit.Blink = 0.0f;
		}
		if (interaction.Hovered && m_LeftPressed)
			m_FocusClaimed = true;
		bool focused = m_FocusId == id;
		if (focused && interaction.Held && !interaction.Pressed)
			m_Edit.Caret = caretFromMouse();
		bool changed = false;
		if (focused)
		{
			EditText(text, maxLength, accept, changed);
			focused = m_FocusId == id;
		}

		const float hover = Animate(id, interaction.Hovered || focused);
		draw.Rect(rect, { .Fill = Hex(0x07060E, 0.88f), .FillBottom = Hex(0x120E24, 0.88f), .Border = focused ? Theme::Astral : glm::mix(WithAlpha(Theme::Edge, 0.75f), Lighten(Theme::Edge, 0.2f), hover), .BorderWidth = 1.2f,
			.Glow = focused ? WithAlpha(Theme::Astral, 0.1f) : UiColor(0.0f), .GlowSize = 10.0f, .Chamfer = 7.0f, .Flow = focused ? 0.7f * m_Motion : 0.0f, .Seed = 0.37f });
		draw.PushClip(area);
		if (focused)
		{
			const float caretX = draw.MeasureWidth(std::string_view(text).substr(0, std::min(m_Edit.Caret, text.size())), style);
			if (caretX - m_Edit.Scroll > area.W - 2.0f)
				m_Edit.Scroll = caretX - area.W + 2.0f;
			if (caretX - m_Edit.Scroll < 0.0f)
				m_Edit.Scroll = caretX;
			if (m_Edit.Anchor != m_Edit.Caret)
			{
				const float anchorX = draw.MeasureWidth(std::string_view(text).substr(0, std::min(m_Edit.Anchor, text.size())), style);
				const float from = std::min(caretX, anchorX) - m_Edit.Scroll;
				const float to = std::max(caretX, anchorX) - m_Edit.Scroll;
				draw.Rect({ area.X + from, rect.Center().y - 11.0f, to - from, 22.0f }, { .Fill = WithAlpha(Theme::Accent, 0.35f), .Radius = 2.0f });
			}
			if (std::fmod(m_Edit.Blink, 1.06f) < 0.6f)
				draw.Rect({ area.X + caretX - m_Edit.Scroll, rect.Center().y - 10.0f, 1.6f, 20.0f }, { .Fill = Theme::Astral });
		}
		const float scroll = focused ? m_Edit.Scroll : 0.0f;
		if (text.empty() && !focused)
			draw.TextAligned(area, placeholder, Theme::Body(17.0f, Theme::Faint), { 0.0f, 0.5f });
		else
			draw.TextAligned({ area.X - scroll, area.Y, area.W + scroll + 4096.0f, area.H }, text, style, { 0.0f, 0.5f });
		draw.PopClip();
		return changed;
	}

	UiTooltip& Ui::Tooltip()
	{
		m_TooltipActive = true;
		m_Tooltip = {};
		return m_Tooltip;
	}

	void Ui::DrawTooltip()
	{
		UiDrawList& draw = Draw(UiLayer::Tooltip);
		constexpr float Padding = 14.0f;
		const float maxContent = m_Tooltip.Width - 2.0f * Padding;
		float contentWidth = 0.0f;
		float height = 0.0f;
		for (UiTooltip::Line& line : m_Tooltip.Lines)
		{
			const float lineHeight = draw.LineHeight(line.Style);
			switch (line.Type)
			{
			case UiTooltip::Kind::Text:
			{
				const float marker = line.Image || line.Icon ? lineHeight * 1.3f + 8.0f : 0.0f;
				float widest = 0.0f;
				const std::vector<TextLine> wrapped = draw.WrapText(line.Left, line.Style, maxContent - marker);
				for (const TextLine& text : wrapped)
					widest = std::max(widest, text.Width);
				contentWidth = std::max(contentWidth, widest + marker);
				line.Height = std::max(float(wrapped.size()) * lineHeight, marker > 0.0f ? lineHeight * 1.3f : 0.0f) + 2.0f;
				break;
			}
			case UiTooltip::Kind::Pair:
				contentWidth = std::max(contentWidth, draw.MeasureWidth(line.Left, line.Style) + draw.MeasureWidth(line.Right, Theme::Body(15.0f)) + 24.0f);
				line.Height = lineHeight + 1.0f;
				break;
			case UiTooltip::Kind::Separator:
				line.Height = 11.0f;
				break;
			case UiTooltip::Kind::Space:
				break;
			}
			height += line.Height;
		}
		const float width = std::min(contentWidth, maxContent) + 2.0f * Padding;
		height += 2.0f * Padding - 2.0f;
		glm::vec2 position = m_Mouse + glm::vec2(20.0f, 22.0f);
		if (position.x + width > m_Size.x - 8.0f)
			position.x = m_Mouse.x - width - 14.0f;
		if (position.y + height > m_Size.y - 8.0f)
			position.y = m_Size.y - height - 8.0f;
		position = glm::max(position, glm::vec2(8.0f));
		const UiRect box{ position.x, position.y, width, height };

		draw.Rect(box.Translate({ 0.0f, 6.0f }).Expand(2.0f), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.65f), .Softness = 16.0f, .Chamfer = 10.0f });
		draw.Rect(box, { .Fill = Hex(0x120E22, 0.97f), .FillBottom = Hex(0x07060E, 0.97f), .Border = m_Tooltip.Accent, .BorderWidth = 1.3f, .Glow = WithAlpha(m_Tooltip.Accent, 0.08f), .GlowSize = 20.0f, .Chamfer = 8.0f, .Aether = 0.5f, .Seed = 0.71f });
		const bool header = !m_Tooltip.Lines.empty() && (m_Tooltip.Lines.front().Image || m_Tooltip.Lines.front().Icon || m_Tooltip.Lines.front().Style.Font == UiFont::Bold);
		if (header)
			draw.Rect({ box.X + 2.0f, box.Y + 2.0f, box.W - 4.0f, m_Tooltip.Lines.front().Height + Padding + 4.0f }, { .Fill = WithAlpha(m_Tooltip.Accent, 0.28f), .FillBottom = WithAlpha(m_Tooltip.Accent, 0.0f), .Chamfer = 7.0f });

		float y = box.Y + Padding;
		const float x = box.X + Padding;
		const float contentRight = box.Right() - Padding;
		for (const UiTooltip::Line& line : m_Tooltip.Lines)
		{
			const float lineHeight = draw.LineHeight(line.Style);
			switch (line.Type)
			{
			case UiTooltip::Kind::Text:
			{
				float textX = x;
				if (line.Image || line.Icon)
				{
					const float size = lineHeight * 1.3f;
					const UiRect marker{ x, y + (line.Height - 2.0f - size) * 0.5f, size, size };
					if (line.Image)
					{
						draw.Shadow(marker.Shrink(4.0f), size * 0.4f, 8.0f, WithAlpha(line.IconColor, 0.45f), { 0.0f, 0.0f });
						draw.Image(*line.Image, marker);
					}
					else
						draw.Icon(*line.Icon, marker, { .Color = Lighten(line.IconColor, 0.25f), .ColorBottom = line.IconColor, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
					textX += size + 8.0f;
				}
				const float wrap = maxContent - (textX - x);
				const float textHeight = float(draw.WrapText(line.Left, line.Style, wrap).size()) * lineHeight;
				draw.TextWrapped({ textX, y + std::max(line.Height - 2.0f - textHeight, 0.0f) * 0.5f, wrap, line.Height }, line.Left, line.Style);
				break;
			}
			case UiTooltip::Kind::Pair:
				draw.Text({ x, y }, line.Left, line.Style);
				draw.TextAligned({ x, y, contentRight - x, lineHeight }, line.Right, Theme::Body(15.0f, line.RightColor), { 1.0f, 0.0f });
				break;
			case UiTooltip::Kind::Separator:
				FadedLine(draw, x - 4.0f, y + 5.0f, contentRight - x + 8.0f, WithAlpha(m_Tooltip.Accent, 0.8f));
				break;
			case UiTooltip::Kind::Space:
				break;
			}
			y += line.Height;
		}
	}
}

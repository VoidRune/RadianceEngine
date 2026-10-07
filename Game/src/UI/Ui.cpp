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
			return { .Font = UiFont::Title, .Size = size, .Color = GoldLight, .ColorBottom = Gold, .Weight = 0.15f, .ShadowOffset = { 0.0f, 1.5f }, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.8f), .ShadowBlur = 2.5f };
		}

		TextStyle Label(float size, UiColor color)
		{
			return { .Font = UiFont::Body, .Size = size, .Color = color };
		}

		TextStyle Floating(float size, UiColor color, UiFont font)
		{
			return { .Font = font, .Size = size, .Color = color, .Outline = 1.6f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.92f), .ShadowOffset = { 0.0f, 2.0f }, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.55f), .ShadowBlur = 3.0f };
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

	void Ui::Panel(const UiRect& rect, float radius, bool blocks)
	{
		UiDrawList& draw = Draw();
		draw.Shadow(rect, radius, 22.0f, Rgba(0.0f, 0.0f, 0.0f, 0.6f), { 0.0f, 8.0f });
		draw.Rect(rect, { .Fill = Theme::PanelTop, .FillBottom = Theme::PanelBottom, .Radius = radius, .Border = WithAlpha(Theme::GoldDark, 0.95f), .BorderWidth = 1.5f });
		draw.Rect(rect.Shrink(4.0f), { .Radius = radius - 3.0f, .Border = Rgba(1.0f, 0.86f, 0.55f, 0.08f), .BorderWidth = 1.0f });
		for (const glm::vec2 corner : { rect.Min(), glm::vec2(rect.Right(), rect.Y), glm::vec2(rect.X, rect.Bottom()), rect.Max() })
		{
			const glm::vec2 inset = glm::vec2(corner.x < rect.Center().x ? 7.0f : -7.0f, corner.y < rect.Center().y ? 7.0f : -7.0f);
			draw.Rect(UiRect::Centered(corner + inset, glm::vec2(5.0f)), { .Fill = Theme::Gold, .FillBottom = Theme::GoldDark, .Radius = 0.8f, .Rotation = Pi * 0.25f });
		}
		if (blocks)
			Block(rect);
	}

	UiRect Ui::Window(UiId id, std::string_view title, UiRect rect, bool* open)
	{
		WindowState& state = m_Windows[id];
		const glm::vec2 home = rect.Min();
		rect = rect.Translate(state.Offset);
		rect.X = std::clamp(rect.X, 0.0f, std::max(m_Size.x - rect.W, 0.0f));
		rect.Y = std::clamp(rect.Y, 0.0f, std::max(m_Size.y - rect.H, 0.0f));
		state.Offset = rect.Min() - home;
		Panel(rect, 10.0f, true);

		UiDrawList& draw = Draw();
		const UiRect header{ rect.X, rect.Y, rect.W, 46.0f };
		const UiId dragId = id ^ 0x5DEECE66Dull;
		if (Interact(dragId, header).Held && m_ActiveId == dragId)
			state.Offset += m_Mouse - m_PreviousMouse;
		draw.Rect(header.Shrink(2.0f, 2.0f), { .Fill = Rgba(0.9f, 0.7f, 0.35f, 0.07f), .FillBottom = Rgba(0.9f, 0.7f, 0.35f, 0.0f), .Radius = 8.0f });
		draw.TextAligned(header.Shrink(20.0f, 0.0f), title, Theme::Title(21.0f), { 0.0f, 0.5f });
		FadedLine(draw, rect.X + 14.0f, header.Bottom() - 1.0f, rect.W - 28.0f, WithAlpha(Theme::Gold, 0.55f));
		if (open)
		{
			const UiRect close{ rect.Right() - 40.0f, rect.Y + 9.0f, 28.0f, 28.0f };
			const UiId closeId = id ^ 0xB5AD4ECEDA1CE2A9ull;
			const UiInteraction interaction = Interact(closeId, close);
			const float hover = Animate(closeId, interaction.Hovered);
			draw.Rect(close, { .Fill = Rgba(1.0f, 0.4f, 0.3f, 0.18f * hover), .Radius = 6.0f });
			draw.Icon(UiIcon::CrossMark, close.Shrink(6.0f), { .Color = glm::mix(Theme::Muted, Theme::Danger, hover) });
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
			FadedLine(draw, rect.X, y, rect.W, WithAlpha(Theme::Gold, 0.4f));
			return;
		}
		const TextStyle style{ .Font = UiFont::Title, .Size = 14.0f, .Color = Theme::Gold, .Weight = 0.1f, .Tracking = 0.6f };
		const float width = draw.MeasureWidth(label, style);
		draw.TextAligned({ rect.X, rect.Y, width, rect.H }, label, style, { 0.0f, 0.5f });
		const float start = rect.X + width + 10.0f;
		if (rect.Right() > start)
			draw.Rect({ start, y, rect.Right() - start, 1.0f }, { .Fill = WithAlpha(Theme::Gold, 0.45f), .FillBottom = WithAlpha(Theme::Gold, 0.0f), .Horizontal = true });
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
		UiColor top = Hex(0x302B33);
		UiColor bottom = Hex(0x1B191E);
		UiColor border = Theme::GoldDark;
		UiColor text = Theme::Text;
		if (kind == ButtonKind::Primary)
		{
			top = Hex(0x9C7535);
			bottom = Hex(0x5A4019);
			border = Theme::Gold;
			text = Hex(0xFFF4D6);
		}
		else if (kind == ButtonKind::Danger)
		{
			top = Hex(0x7C302B);
			bottom = Hex(0x461814);
			border = Hex(0xC4584E);
		}
		top = Lighten(top, 0.12f * hover);
		bottom = Lighten(bottom, 0.06f * hover);
		border = glm::mix(border, Theme::GoldLight, 0.45f * hover);
		if (pressed)
		{
			top = Darken(top, 0.18f);
			bottom = Darken(bottom, 0.1f);
		}
		if (!enabled)
		{
			top = WithAlpha(Darken(top, 0.3f), 0.8f);
			bottom = WithAlpha(Darken(bottom, 0.3f), 0.8f);
			border = WithAlpha(border, 0.35f);
			text = Theme::Faint;
		}
		UiDrawList& draw = Draw();
		const UiRect body = rect.Translate({ 0.0f, pressed ? 1.0f : 0.0f });
		draw.Shadow(rect, 6.0f, 6.0f, Rgba(0.0f, 0.0f, 0.0f, 0.45f), { 0.0f, 2.0f });
		draw.Rect(body, { .Fill = top, .FillBottom = bottom, .Radius = 6.0f, .Border = border, .BorderWidth = 1.3f, .Glow = WithAlpha(Theme::GoldLight, 0.22f * hover), .GlowSize = 10.0f });
		FadedLine(draw, body.X + 4.0f, body.Y + 1.5f, body.W - 8.0f, Rgba(1.0f, 0.95f, 0.8f, 0.14f + 0.1f * hover));
		const TextStyle style = Theme::Bold(17.0f, text);
		const float textWidth = draw.MeasureWidth(label, style);
		const float iconSize = icon ? 20.0f : 0.0f;
		const float gap = icon && !label.empty() ? 8.0f : 0.0f;
		const float start = body.Center().x - (iconSize + gap + textWidth) * 0.5f;
		if (icon)
			draw.Icon(*icon, UiRect::Centered({ start + iconSize * 0.5f, body.Center().y }, glm::vec2(iconSize)), { .Color = text, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.5f) });
		draw.TextAligned({ start + iconSize + gap, body.Y, textWidth + 2.0f, body.H }, label, { .Font = style.Font, .Size = style.Size, .Color = text, .ShadowOffset = { 0.0f, 1.0f }, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f), .ShadowBlur = 1.5f }, { 0.0f, 0.5f });
		return enabled && interaction.Clicked;
	}

	bool Ui::Checkbox(UiId id, const UiRect& rect, std::string_view label, bool& value)
	{
		const UiInteraction interaction = Interact(id, rect);
		const float hover = Animate(id, interaction.Hovered);
		UiDrawList& draw = Draw();
		const UiRect box{ rect.X, rect.Center().y - 10.0f, 20.0f, 20.0f };
		draw.Rect(box, { .Fill = Theme::SlotTop, .FillBottom = Theme::SlotBottom, .Radius = 4.0f, .Border = glm::mix(Theme::GoldDark, Theme::Gold, hover), .BorderWidth = 1.3f });
		if (value)
			draw.Icon(UiIcon::CheckMark, box.Shrink(2.5f), { .Color = Theme::GoldLight, .ColorBottom = Theme::Gold });
		draw.TextAligned({ box.Right() + 10.0f, rect.Y, rect.W - box.W - 10.0f, rect.H }, label, Theme::Body(16.0f, glm::mix(Theme::Text, Theme::GoldLight, hover * 0.6f)), { 0.0f, 0.5f });
		if (interaction.Clicked)
		{
			value = !value;
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
		const UiRect track{ area.X + 8.0f, area.Center().y - 3.0f, area.W - 16.0f, 6.0f };
		bool changed = false;
		if (interaction.Held && max > min)
		{
			const float next = min + std::clamp((m_Mouse.x - track.X) / std::max(track.W, 1.0f), 0.0f, 1.0f) * (max - min);
			changed = next != value;
			value = next;
		}
		const float fraction = max > min ? std::clamp((value - min) / (max - min), 0.0f, 1.0f) : 0.0f;
		UiDrawList& draw = Draw();
		draw.Rect(track, { .Fill = Theme::SlotBottom, .Radius = 3.0f, .Border = WithAlpha(Theme::GoldDark, 0.7f), .BorderWidth = 1.0f });
		draw.Rect({ track.X, track.Y, track.W * fraction, track.H }, { .Fill = Theme::GoldDark, .FillBottom = Theme::Gold, .Radius = 3.0f, .Horizontal = true });
		const glm::vec2 knob(track.X + track.W * fraction, track.Center().y);
		draw.Circle(knob, 8.0f + hover, Lighten(Theme::Gold, 0.2f * hover), Hex(0x3A2A10), 1.5f);
		draw.TextAligned(label, text, Theme::Body(15.0f, Theme::Muted), { 1.0f, 0.5f });
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
		draw.Rect(rect, { .Fill = Theme::SlotBottom, .FillBottom = Theme::SlotTop, .Radius = 6.0f, .Border = focused ? Theme::Gold : glm::mix(WithAlpha(Theme::GoldDark, 0.8f), Theme::GoldDark * 1.4f, hover), .BorderWidth = 1.3f, .Glow = focused ? WithAlpha(Theme::Gold, 0.12f) : UiColor(0.0f), .GlowSize = 8.0f });
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
				draw.Rect({ area.X + from, rect.Center().y - 11.0f, to - from, 22.0f }, { .Fill = WithAlpha(Theme::Gold, 0.3f), .Radius = 2.0f });
			}
			if (std::fmod(m_Edit.Blink, 1.06f) < 0.6f)
				draw.Rect({ area.X + caretX - m_Edit.Scroll, rect.Center().y - 10.0f, 1.6f, 20.0f }, { .Fill = Theme::GoldLight });
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

		draw.Shadow(box, 8.0f, 16.0f, Rgba(0.0f, 0.0f, 0.0f, 0.65f), { 0.0f, 6.0f });
		draw.Rect(box, { .Fill = Hex(0x18161B, 0.98f), .FillBottom = Hex(0x0F0E12, 0.98f), .Radius = 8.0f, .Border = m_Tooltip.Accent, .BorderWidth = 1.5f });
		const bool header = !m_Tooltip.Lines.empty() && (m_Tooltip.Lines.front().Image || m_Tooltip.Lines.front().Icon || m_Tooltip.Lines.front().Style.Font == UiFont::Bold);
		if (header)
			draw.Rect({ box.X + 2.0f, box.Y + 2.0f, box.W - 4.0f, m_Tooltip.Lines.front().Height + Padding + 4.0f }, { .Fill = WithAlpha(m_Tooltip.Accent, 0.28f), .FillBottom = WithAlpha(m_Tooltip.Accent, 0.0f), .Radius = 6.0f });

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
				const float textHeight = float(draw.WrapText(line.Left, line.Style, contentRight - textX).size()) * lineHeight;
				draw.TextWrapped({ textX, y + std::max(line.Height - 2.0f - textHeight, 0.0f) * 0.5f, contentRight - textX, line.Height }, line.Left, line.Style);
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

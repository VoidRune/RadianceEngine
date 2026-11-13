#pragma once
#include "UI/UiDrawList.h"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Rdn
{
	class Window;
	enum class KeyCode;
}

namespace Game
{
	using UiId = uint64_t;

	enum class UiLayer : uint8_t
	{
		Background,
		World,
		Hud,
		Windows,
		Popup,
		Tooltip,
		Drag,
		Overlay,
		Count,
	};

	enum class ButtonKind : uint8_t
	{
		Normal,
		Primary,
		Danger,
	};

	struct UiInteraction
	{
		bool Hovered = false;
		bool Pressed = false;
		bool Clicked = false;
		bool RightClicked = false;
		bool Held = false;
	};

	struct UiPayload
	{
		int Type = 0;
		int Value = 0;
		UiId Source = 0;
	};

	namespace Theme
	{
		inline const UiColor Text = Hex(0xEEE8DC);
		inline const UiColor Muted = Hex(0xA8A1B2);
		inline const UiColor Faint = Hex(0x6E687E);
		inline const UiColor Edge = Hex(0x857455);
		inline const UiColor Accent = Hex(0xD6BC84);
		inline const UiColor AccentLight = Hex(0xF3E5C2);
		inline const UiColor AccentDark = Hex(0x3A3122);
		inline const UiColor Astral = Hex(0x9FDCEA);
		inline const UiColor Gold = Hex(0xF2C978);
		inline const UiColor GoldLight = Hex(0xFFF1CF);
		inline const UiColor GoldDark = Hex(0x8E6C36);
		inline const UiColor Ink = Hex(0x05050A);
		inline const UiColor PanelTop = Hex(0x0D0E1A, 0.86f);
		inline const UiColor PanelBottom = Hex(0x07070F, 0.9f);
		inline const UiColor CardTop = Hex(0x13141F, 0.82f);
		inline const UiColor CardBottom = Hex(0x0B0B13, 0.86f);
		inline const UiColor SlotTop = Hex(0x12131D);
		inline const UiColor SlotBottom = Hex(0x08080D);
		inline const UiColor Danger = Hex(0xF0646E);
		inline const UiColor Success = Hex(0x83E0A6);
		inline const UiColor Warning = Hex(0xF2C46A);
		inline const UiColor Mana = Hex(0x6F8DFF);
		inline const UiColor Health = Hex(0xE04B5A);

		TextStyle Body(float size = 16.0f, UiColor color = Text);
		TextStyle Bold(float size = 16.0f, UiColor color = Text);
		TextStyle Title(float size = 22.0f);
		TextStyle Label(float size = 15.0f, UiColor color = Muted);
		TextStyle Floating(float size, UiColor color, UiFont font = UiFont::Bold);
		TextStyle Caps(float size, UiColor color, float tracking = 1.5f);
	}

	class UiTooltip
	{
	public:
		enum class Kind : uint8_t
		{
			Text,
			Pair,
			Separator,
			Space,
		};

		struct Line
		{
			Kind Type = Kind::Text;
			std::string Left;
			std::string Right;
			TextStyle Style;
			UiColor RightColor{ 1.0f };
			std::optional<UiImage> Image;
			std::optional<UiIcon> Icon;
			UiColor IconColor{ 1.0f };
			float Height = 0.0f;
		};

		UiTooltip& Header(std::string text, UiColor color, std::optional<UiImage> image = std::nullopt, std::optional<UiIcon> icon = std::nullopt);
		UiTooltip& Text(std::string text, UiColor color = Theme::Text, float size = 15.0f);
		UiTooltip& Muted(std::string text, float size = 14.0f) { return Text(std::move(text), Theme::Muted, size); }
		UiTooltip& Pair(std::string left, std::string right, UiColor leftColor = Theme::Muted, UiColor rightColor = Theme::Text);
		UiTooltip& Separator();
		UiTooltip& Space(float height = 4.0f);

		UiColor Accent = Theme::Edge;
		float Width = 330.0f;
		std::vector<Line> Lines;
	};

	class Ui
	{
	public:
		Ui(const UiAssets& assets, Rdn::Window& window);

		void BeginFrame(float dt, glm::vec2 framebufferSize, bool inputAllowed);
		void EndFrame();
		void Collect(std::vector<UiPrimitive>& primitives) const;

		UiDrawList& Draw() { return m_Lists[size_t(m_Layer)]; }
		UiDrawList& Draw(UiLayer layer) { return m_Lists[size_t(layer)]; }
		void SetLayer(UiLayer layer) { m_Layer = layer; }
		UiLayer GetLayer() const { return m_Layer; }

		glm::vec2 GetSize() const { return m_Size; }
		UiRect GetScreen() const { return { 0.0f, 0.0f, m_Size.x, m_Size.y }; }
		float GetScale() const { return m_Scale; }
		float GetTime() const { return m_Time; }
		float GetDeltaTime() const { return m_DeltaTime; }
		void SetMotion(float motion) { m_Motion = motion; }
		void SetOpacity(float opacity) { m_Opacity = opacity; }
		float GetOpacity() const { return m_Opacity; }
		glm::vec2 GetMouse() const { return m_Mouse; }
		glm::vec2 ToLogical(glm::vec2 pixels) const { return pixels / m_Scale; }

		static UiId MakeId(std::string_view name, uint64_t index = 0);
		UiInteraction Interact(UiId id, const UiRect& rect);
		void Block(const UiRect& rect);
		bool WantsMouse() const;
		bool WantsKeyboard() const { return m_FocusId != 0; }
		uint32_t TakeClicks() { return std::exchange(m_Clicks, 0u); }
		int TakeHover() { return std::exchange(m_HoverEntry, -1); }
		float Animate(UiId id, bool target, float speed = 10.0f);
		float Since(UiId id, bool active);

		void OfferDrag(UiId id, int type, int value);
		const UiPayload* GetDrag() const { return m_Drag ? &*m_Drag : nullptr; }
		std::optional<int> AcceptDrop(const UiRect& rect, int type);

		void Panel(const UiRect& rect, float chamfer = 14.0f, bool blocks = true);
		void Card(const UiRect& rect, float hover, float selected, UiColor accent = Theme::Accent);
		UiRect Window(UiId id, std::string_view title, UiRect rect, bool* open = nullptr);
		void Separator(const UiRect& rect, std::string_view label = {});
		bool Button(UiId id, const UiRect& rect, std::string_view label, ButtonKind kind = ButtonKind::Normal, bool enabled = true, std::optional<UiIcon> icon = std::nullopt);
		bool MenuEntry(UiId id, const UiRect& rect, std::string_view title, std::string_view subtitle, bool selected = false, bool centered = false);
		bool Checkbox(UiId id, const UiRect& rect, std::string_view label, bool& value);
		bool Slider(UiId id, const UiRect& rect, float& value, float min, float max, std::string_view text);
		bool Choice(UiId id, const UiRect& rect, int& index, std::span<const char* const> options);
		bool Tabs(UiId id, const UiRect& rect, int& index, std::span<const char* const> labels);
		bool TextField(UiId id, const UiRect& rect, std::string& text, size_t maxLength, std::string_view placeholder = {}, bool (*accept)(uint32_t) = nullptr);
		UiTooltip& Tooltip();

	private:
		struct Region
		{
			UiId Id = 0;
			UiRect Rect;
			UiLayer Layer = UiLayer::Background;
			uint32_t Order = 0;
		};

		struct WindowState
		{
			glm::vec2 Offset{ 0.0f };
		};

		struct EditState
		{
			size_t Caret = 0;
			size_t Anchor = 0;
			float Scroll = 0.0f;
			float Blink = 0.0f;
		};

		void Register(UiId id, const UiRect& rect);
		bool Repeat(Rdn::KeyCode key);
		void EditText(std::string& text, size_t maxLength, bool (*accept)(uint32_t), bool& changed);
		void DrawTooltip();

		const UiAssets& m_Assets;
		Rdn::Window& m_Window;
		std::array<UiDrawList, size_t(UiLayer::Count)> m_Lists;
		UiLayer m_Layer = UiLayer::Hud;
		glm::vec2 m_Size{ 1.0f };
		float m_Scale = 1.0f;
		float m_Time = 0.0f;
		float m_DeltaTime = 0.0f;
		float m_Motion = 0.0f;
		float m_Opacity = 1.0f;
		glm::vec2 m_Mouse{ 0.0f };
		glm::vec2 m_PreviousMouse{ 0.0f };
		glm::vec2 m_PressPosition{ 0.0f };
		bool m_InputAllowed = true;
		bool m_LeftPressed = false;
		bool m_LeftReleased = false;
		uint32_t m_Clicks = 0;
		int m_HoverEntry = -1;
		int m_EntryCount = 0;
		UiId m_EntryHovered = 0;
		UiId m_EntryHoveredBefore = 0;
		bool m_LeftDown = false;
		bool m_RightPressed = false;
		std::vector<Region> m_Regions;
		UiId m_HoverId = 0;
		bool m_MouseOverUi = false;
		UiId m_ActiveId = 0;
		UiId m_FocusId = 0;
		UiId m_FocusRequest = 0;
		bool m_FocusClaimed = false;
		EditState m_Edit;
		std::optional<UiPayload> m_Drag;
		bool m_DropAccepted = false;
		std::unordered_map<UiId, WindowState> m_Windows;
		std::unordered_map<UiId, float> m_Animations;
		std::unordered_map<UiId, float> m_Timers;
		std::unordered_map<int, float> m_RepeatTimers;
		UiTooltip m_Tooltip;
		bool m_TooltipActive = false;
	};
}

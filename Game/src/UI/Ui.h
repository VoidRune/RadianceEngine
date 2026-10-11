#pragma once
#include "UI/UiDrawList.h"
#include <array>
#include <cstdint>
#include <optional>
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
		inline const UiColor Text = Hex(0xEAE3D2);
		inline const UiColor Muted = Hex(0xA39B8C);
		inline const UiColor Faint = Hex(0x716B60);
		inline const UiColor Gold = Hex(0xE3B456);
		inline const UiColor GoldLight = Hex(0xF8E3A3);
		inline const UiColor GoldDark = Hex(0x7A5C2A);
		inline const UiColor PanelTop = Hex(0x201D23, 0.97f);
		inline const UiColor PanelBottom = Hex(0x131216, 0.97f);
		inline const UiColor SlotTop = Hex(0x141317);
		inline const UiColor SlotBottom = Hex(0x0C0B0E);
		inline const UiColor Danger = Hex(0xE8665C);
		inline const UiColor Success = Hex(0x7FD884);
		inline const UiColor Warning = Hex(0xF2B95A);
		inline const UiColor Mana = Hex(0x5E8DFF);
		inline const UiColor Health = Hex(0xD84848);

		TextStyle Body(float size = 16.0f, UiColor color = Text);
		TextStyle Bold(float size = 16.0f, UiColor color = Text);
		TextStyle Title(float size = 22.0f);
		TextStyle Label(float size = 15.0f, UiColor color = Muted);
		TextStyle Floating(float size, UiColor color, UiFont font = UiFont::Bold);
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

		UiColor Accent = Theme::GoldDark;
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
		glm::vec2 GetMouse() const { return m_Mouse; }
		glm::vec2 ToLogical(glm::vec2 pixels) const { return pixels / m_Scale; }

		static UiId MakeId(std::string_view name, uint64_t index = 0);
		UiInteraction Interact(UiId id, const UiRect& rect);
		void Block(const UiRect& rect);
		bool WantsMouse() const;
		bool WantsKeyboard() const { return m_FocusId != 0; }
		uint32_t TakeClicks() { return std::exchange(m_Clicks, 0u); }
		float Animate(UiId id, bool target, float speed = 10.0f);

		void OfferDrag(UiId id, int type, int value);
		const UiPayload* GetDrag() const { return m_Drag ? &*m_Drag : nullptr; }
		std::optional<int> AcceptDrop(const UiRect& rect, int type);

		void Panel(const UiRect& rect, float radius = 10.0f, bool blocks = true);
		UiRect Window(UiId id, std::string_view title, UiRect rect, bool* open = nullptr);
		void Separator(const UiRect& rect, std::string_view label = {});
		bool Button(UiId id, const UiRect& rect, std::string_view label, ButtonKind kind = ButtonKind::Normal, bool enabled = true, std::optional<UiIcon> icon = std::nullopt);
		bool Checkbox(UiId id, const UiRect& rect, std::string_view label, bool& value);
		bool Slider(UiId id, const UiRect& rect, float& value, float min, float max, std::string_view text);
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
		glm::vec2 m_Mouse{ 0.0f };
		glm::vec2 m_PreviousMouse{ 0.0f };
		glm::vec2 m_PressPosition{ 0.0f };
		bool m_InputAllowed = true;
		bool m_LeftPressed = false;
		bool m_LeftReleased = false;
		uint32_t m_Clicks = 0;
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
		std::unordered_map<int, float> m_RepeatTimers;
		UiTooltip m_Tooltip;
		bool m_TooltipActive = false;
	};
}

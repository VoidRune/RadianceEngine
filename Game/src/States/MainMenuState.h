#pragma once
#include "App/AppState.h"
#include "UI/UiTypes.h"
#include <string>

namespace Game
{
	class GameApp;
	class Ui;

	class MainMenuState final : public AppState
	{
	public:
		explicit MainMenuState(GameApp& app, std::string message = {});

		void Update(float dt) override;
		void DrawUi() override;
		bool Render(Rdn::RenderGraph& graph, Rdn::RGImage target, Rdn::Extent2D extent, uint32_t frameIndex) override;
		bool WantsVsync() const override { return true; }

	private:
		enum class Page : uint8_t
		{
			None,
			Host,
			Join,
			Character,
			Settings,
		};

		void StartSession(bool host);
		void RefreshCharacter();
		void OpenPage(Page page);
		void DrawMotes(Ui& ui);
		void DrawTitle(Ui& ui);
		void DrawMenu(Ui& ui, UiRect area);
		void DrawPage(Ui& ui);
		void DrawHostPage(Ui& ui, UiRect area);
		void DrawJoinPage(Ui& ui, UiRect area);
		void DrawCharacterPage(Ui& ui, UiRect area);

		GameApp& m_App;
		std::string m_Message;
		std::string m_Name;
		std::string m_Address;
		std::string m_Port;
		int m_Kit = 0;
		bool m_Fresh = false;
		bool m_HasSave = false;
		std::string m_CheckedName;
		std::string m_SaveSummary;
		bool m_Unlimited = false;
		Page m_Page = Page::None;
		float m_Reveal = 0.0f;
		float m_Time = 0.0f;
		glm::vec2 m_Parallax{ 0.0f };
	};
}

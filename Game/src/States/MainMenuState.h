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

		void DrawUi() override;

	private:
		void StartSession(bool host);
		void RefreshCharacter();
		void DrawBackground(Ui& ui);
		void DrawCharacterPanel(Ui& ui, UiRect area);
		void DrawPlayPanel(Ui& ui, UiRect area);

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
		float m_Volume = 0.8f;
		float m_MusicVolume = 0.8f;
		float m_EffectsVolume = 1.0f;
		bool m_Muted = false;
		float m_LagMs = 0.0f;
		float m_LossPercent = 0.0f;
	};
}

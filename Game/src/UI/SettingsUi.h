#pragma once
#include "UI/UiTypes.h"
#include <GameShared/Content/Cosmetics.h>

namespace Game
{
	class GameApp;
	class Ui;

	void DrawSettings(Ui& ui, GameApp& app, UiRect area);
	bool DrawOutfitPicker(Ui& ui, UiRect row, Outfit& outfit);
	bool DrawFocusPicker(Ui& ui, UiRect row, Focus& focus);
	bool DrawAuraPicker(Ui& ui, UiRect row, Aura& aura);
	bool DrawHeadpiecePicker(Ui& ui, UiRect row, Headpiece& headpiece);
}

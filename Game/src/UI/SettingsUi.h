#pragma once
#include "UI/UiTypes.h"
#include <GameShared/Content/Cosmetics.h>

namespace Game
{
	class GameApp;
	class Ui;

	void DrawSettings(Ui& ui, GameApp& app, UiRect area);
	bool DrawHeadwearPicker(Ui& ui, UiRect row, Headwear& headwear);
}

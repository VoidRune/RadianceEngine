#pragma once
#include "KeyCodes.h"

namespace Rdn
{
	class Input
	{
	public:
		static bool IsKeyDown(KeyCode keyCode);
		static bool IsKeyUp(KeyCode keyCode);
		static bool IsKeyPressed(KeyCode keyCode);
		static bool IsKeyReleased(KeyCode keyCode);

		static float GetScrollVertical();
		static float GetScrollHorizontal();
		static float GetMouseX();
		static float GetMouseY();

	private:
		static void TransitionFrame();
		static void UpdateMouseAndScroll(float mouseX, float mouseY, float scrollX, float scrollY);
		static void SetKey(KeyCode keyCode, bool isDown);

		friend class Window;
	};
}
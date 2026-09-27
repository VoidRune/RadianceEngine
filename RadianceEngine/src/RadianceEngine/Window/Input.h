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

		static float GetMouseX();
		static float GetMouseY();
		static float GetMouseDeltaX();
		static float GetMouseDeltaY();
		static float GetScrollVertical();
		static float GetScrollHorizontal();

	private:
		static void BeginFrame();
		static void SetKey(int key, bool isDown);
		static void SetMousePosition(double x, double y);
		static void MoveMouse(double x, double y);
		static void AddScroll(double x, double y);

		friend class Window;
	};
}

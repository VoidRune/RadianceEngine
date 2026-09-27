#include "Input.h"
#include <bitset>

namespace Rdn
{
	namespace
	{
		constexpr size_t KeyCount = static_cast<size_t>(KeyCode::KeyCount);

		std::bitset<KeyCount> s_Down;
		std::bitset<KeyCount> s_Pressed;
		std::bitset<KeyCount> s_Released;

		double s_MouseX = 0.0;
		double s_MouseY = 0.0;
		double s_MouseDeltaX = 0.0;
		double s_MouseDeltaY = 0.0;
		double s_ScrollX = 0.0;
		double s_ScrollY = 0.0;

		bool Test(const std::bitset<KeyCount>& keys, KeyCode keyCode)
		{
			const size_t index = static_cast<size_t>(keyCode);
			return index < KeyCount && keys[index];
		}
	}

	void Input::BeginFrame()
	{
		s_Pressed.reset();
		s_Released.reset();
		s_MouseDeltaX = 0.0;
		s_MouseDeltaY = 0.0;
		s_ScrollX = 0.0;
		s_ScrollY = 0.0;
	}

	void Input::SetKey(int key, bool isDown)
	{
		if (key < 0 || key >= static_cast<int>(KeyCount))
			return;

		s_Down[key] = isDown;
		if (isDown)
			s_Pressed[key] = true;
		else
			s_Released[key] = true;
	}

	void Input::SetMousePosition(double x, double y)
	{
		s_MouseX = x;
		s_MouseY = y;
	}

	void Input::MoveMouse(double x, double y)
	{
		s_MouseDeltaX += x - s_MouseX;
		s_MouseDeltaY += y - s_MouseY;
		s_MouseX = x;
		s_MouseY = y;
	}

	void Input::AddScroll(double x, double y)
	{
		s_ScrollX += x;
		s_ScrollY += y;
	}

	bool Input::IsKeyDown(KeyCode keyCode) { return Test(s_Down, keyCode); }
	bool Input::IsKeyUp(KeyCode keyCode) { return !Test(s_Down, keyCode); }
	bool Input::IsKeyPressed(KeyCode keyCode) { return Test(s_Pressed, keyCode); }
	bool Input::IsKeyReleased(KeyCode keyCode) { return Test(s_Released, keyCode); }

	float Input::GetMouseX() { return static_cast<float>(s_MouseX); }
	float Input::GetMouseY() { return static_cast<float>(s_MouseY); }
	float Input::GetMouseDeltaX() { return static_cast<float>(s_MouseDeltaX); }
	float Input::GetMouseDeltaY() { return static_cast<float>(s_MouseDeltaY); }
	float Input::GetScrollVertical() { return static_cast<float>(s_ScrollY); }
	float Input::GetScrollHorizontal() { return static_cast<float>(s_ScrollX); }
}

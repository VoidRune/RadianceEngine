#include "Input.h"
#include <bitset>

namespace Rdn
{
	constexpr size_t MaxKeys = static_cast<size_t>(KeyCode::KeyCount);
	static std::bitset<MaxKeys> s_PreviousInput;
	static std::bitset<MaxKeys> s_CurrentInput;

	static float s_ScrollX = 0.0f;
	static float s_ScrollY = 0.0f;
	static float s_MouseX = 0.0f;
	static float s_MouseY = 0.0f;

	void Input::TransitionFrame()
	{
		s_PreviousInput = s_CurrentInput;
	}

	void Input::UpdateMouseAndScroll(float mouseX, float mouseY, float scrollX, float scrollY)
	{
		s_MouseX = mouseX;
		s_MouseY = mouseY;
		s_ScrollX = scrollX;
		s_ScrollY = scrollY;
	}

	void Input::SetKey(KeyCode keyCode, bool isDown)
	{
		s_CurrentInput[static_cast<size_t>(keyCode)] = isDown;
	}

	bool Input::IsKeyDown(KeyCode keyCode) 
	{ 
		return s_CurrentInput[static_cast<size_t>(keyCode)]; 
	}

	bool Input::IsKeyUp(KeyCode keyCode) 
	{ 
		return !s_CurrentInput[static_cast<size_t>(keyCode)]; 
	}

	bool Input::IsKeyPressed(KeyCode keyCode)
	{
		return s_CurrentInput[static_cast<size_t>(keyCode)] && !s_PreviousInput[static_cast<size_t>(keyCode)];
	}

	bool Input::IsKeyReleased(KeyCode keyCode)
	{
		return !s_CurrentInput[static_cast<size_t>(keyCode)] && s_PreviousInput[static_cast<size_t>(keyCode)];
	}

	float Input::GetScrollVertical() { return s_ScrollY; }
	float Input::GetScrollHorizontal() { return s_ScrollX; }
	float Input::GetMouseX() { return s_MouseX; }
	float Input::GetMouseY() { return s_MouseY; }
}
#pragma once
#include <string>
#include <vector>
#include <memory>

namespace Rdn
{
	struct WindowDescription
	{
	public:
		std::string Title = "Application";
		uint32_t Width = 1280;
		uint32_t Height = 720;
		bool Fullscreen = false;
		bool Titlebar = true;
	};

	class Window
	{
	public:
		Window(const WindowDescription& desc);
		~Window();

		void PollEvents();
		void WaitEvents();

		void SetClosed(bool close);
		bool IsClosed() const;
		void SetFullscreen(bool fullscreen);
		bool IsFullscreen() const { return m_IsFullscreen; }

		int Width() const;
		int Height() const;

		void* GetHandle() const { return m_Window; }
		std::vector<const char*> GetInstanceExtensions() const;

	private:
		void InitializeWindow(const WindowDescription& desc);
		void SetupCallbacks();

		void* m_Window = nullptr;
		bool m_IsFullscreen = false;

		int m_WindowedPos[2] = { 0, 0 };
		int m_WindowedSize[2] = { 0, 0 };

		float m_MouseX = 0.0f;
		float m_MouseY = 0.0f;
		float m_ScrollX = 0.0f;
		float m_ScrollY = 0.0f;
	};
}
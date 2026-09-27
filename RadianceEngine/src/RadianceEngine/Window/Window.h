#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct GLFWwindow;

namespace Rdn
{
	struct WindowDescription
	{
		std::string Title = "Application";
		uint32_t Width = 1280;
		uint32_t Height = 720;
		bool Fullscreen = false;
		bool Titlebar = true;
	};

	enum class CursorMode
	{
		Normal,
		Hidden,
		Captured,
		Disabled,
	};

	class Window
	{
	public:
		explicit Window(const WindowDescription& desc);
		~Window();
		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		void PollEvents();
		void WaitEvents();

		void SetClosed(bool close);
		bool IsClosed() const;
		void SetFullscreen(bool fullscreen);
		bool IsFullscreen() const { return m_IsFullscreen; }
		bool IsMinimized() const;
		void SetTitle(const std::string& title);
		void SetCursorMode(CursorMode mode);
		CursorMode GetCursorMode() const { return m_CursorMode; }

		uint32_t Width() const;
		uint32_t Height() const;

		void* GetHandle() const { return m_Window; }
		std::vector<const char*> GetInstanceExtensions() const;

	private:
		void SetupCallbacks();

		GLFWwindow* m_Window = nullptr;
		bool m_Titlebar = true;
		bool m_IsFullscreen = false;
		bool m_RestoreMaximized = false;
		CursorMode m_CursorMode = CursorMode::Normal;
		int m_WindowedPos[2] = { 0, 0 };
		int m_WindowedSize[2] = { 0, 0 };
	};
}

#include "Window.h"
#include "Input.h"
#include "RadianceEngine/Core/Log.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdlib>

namespace Rdn
{
	namespace
	{
		uint32_t s_WindowCount = 0;

		int ToGlfwCursorMode(CursorMode mode)
		{
			switch (mode)
			{
			case CursorMode::Hidden:   return GLFW_CURSOR_HIDDEN;
			case CursorMode::Captured: return GLFW_CURSOR_CAPTURED;
			case CursorMode::Disabled: return GLFW_CURSOR_DISABLED;
			default:                   return GLFW_CURSOR_NORMAL;
			}
		}

		GLFWmonitor* FindMonitor(GLFWwindow* window)
		{
			int x, y, width, height;
			glfwGetWindowPos(window, &x, &y);
			glfwGetWindowSize(window, &width, &height);

			GLFWmonitor* best = glfwGetPrimaryMonitor();
			int64_t bestArea = 0;
			int count = 0;
			GLFWmonitor** monitors = glfwGetMonitors(&count);
			for (int i = 0; i < count; i++)
			{
				const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
				if (!mode)
					continue;
				int monitorX, monitorY;
				glfwGetMonitorPos(monitors[i], &monitorX, &monitorY);
				const int64_t overlapX = std::min(x + width, monitorX + mode->width) - std::max(x, monitorX);
				const int64_t overlapY = std::min(y + height, monitorY + mode->height) - std::max(y, monitorY);
				if (overlapX > 0 && overlapY > 0 && overlapX * overlapY > bestArea)
				{
					bestArea = overlapX * overlapY;
					best = monitors[i];
				}
			}
			return best;
		}
	}

	Window::Window(const WindowDescription& desc)
		: m_Titlebar(desc.Titlebar)
	{
		if (s_WindowCount == 0)
		{
			glfwSetErrorCallback([](int error, const char* description)
				{
					RDN_LOG_ERROR("GLFW error {:#x}: {}", error, description);
				});
			if (!glfwInit())
			{
				RDN_LOG_FATAL("Failed to initialize GLFW");
				std::abort();
			}
		}

		glfwDefaultWindowHints();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		glfwWindowHint(GLFW_DECORATED, desc.Titlebar ? GLFW_TRUE : GLFW_FALSE);

		const int width = static_cast<int>(desc.Width);
		const int height = static_cast<int>(desc.Height);
		m_Window = glfwCreateWindow(width, height, desc.Title.c_str(), nullptr, nullptr);
		if (!m_Window)
		{
			RDN_LOG_FATAL("Failed to create a {}x{} window", desc.Width, desc.Height);
			std::abort();
		}
		s_WindowCount++;

		if (GLFWmonitor* monitor = glfwGetPrimaryMonitor())
		{
			int workX, workY, workWidth, workHeight;
			glfwGetMonitorWorkarea(monitor, &workX, &workY, &workWidth, &workHeight);
			int left, top, right, bottom;
			glfwGetWindowFrameSize(m_Window, &left, &top, &right, &bottom);
			glfwSetWindowPos(m_Window,
				workX + left + std::max(0, (workWidth - width - left - right) / 2),
				workY + top + std::max(0, (workHeight - height - top - bottom) / 2));
		}

		if (glfwRawMouseMotionSupported())
			glfwSetInputMode(m_Window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
		SetupCallbacks();

		if (desc.Fullscreen)
			SetFullscreen(true);
		glfwShowWindow(m_Window);

		double mouseX, mouseY;
		glfwGetCursorPos(m_Window, &mouseX, &mouseY);
		Input::SetMousePosition(mouseX, mouseY);
	}

	Window::~Window()
	{
		glfwDestroyWindow(m_Window);
		if (--s_WindowCount == 0)
			glfwTerminate();
	}

	void Window::SetupCallbacks()
	{
		glfwSetKeyCallback(m_Window, [](GLFWwindow*, int key, int, int action, int)
			{
				if (action != GLFW_REPEAT)
					Input::SetKey(key, action == GLFW_PRESS);
			});

		glfwSetMouseButtonCallback(m_Window, [](GLFWwindow*, int button, int action, int)
			{
				Input::SetKey(button, action == GLFW_PRESS);
			});

		glfwSetCursorPosCallback(m_Window, [](GLFWwindow*, double x, double y)
			{
				Input::MoveMouse(x, y);
			});

		glfwSetScrollCallback(m_Window, [](GLFWwindow*, double x, double y)
			{
				Input::AddScroll(x, y);
			});
	}

	void Window::PollEvents()
	{
		Input::BeginFrame();
		glfwPollEvents();
	}

	void Window::WaitEvents()
	{
		glfwWaitEvents();
	}

	void Window::SetClosed(bool close)
	{
		glfwSetWindowShouldClose(m_Window, close ? GLFW_TRUE : GLFW_FALSE);
	}

	bool Window::IsClosed() const
	{
		return glfwWindowShouldClose(m_Window) == GLFW_TRUE;
	}

	void Window::SetFullscreen(bool fullscreen)
	{
		if (m_IsFullscreen == fullscreen)
			return;

		if (fullscreen)
		{
			if (glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED))
				glfwRestoreWindow(m_Window);
			m_RestoreMaximized = glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED) == GLFW_TRUE;
			if (m_RestoreMaximized)
				glfwRestoreWindow(m_Window);

			GLFWmonitor* monitor = FindMonitor(m_Window);
			const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
			if (!mode)
				return;

			glfwGetWindowPos(m_Window, &m_WindowedPos[0], &m_WindowedPos[1]);
			glfwGetWindowSize(m_Window, &m_WindowedSize[0], &m_WindowedSize[1]);
			int monitorX, monitorY;
			glfwGetMonitorPos(monitor, &monitorX, &monitorY);
			glfwSetWindowAttrib(m_Window, GLFW_DECORATED, GLFW_FALSE);
			glfwSetWindowMonitor(m_Window, nullptr, monitorX, monitorY, mode->width, mode->height, GLFW_DONT_CARE);
		}
		else
		{
			glfwSetWindowAttrib(m_Window, GLFW_DECORATED, m_Titlebar ? GLFW_TRUE : GLFW_FALSE);
			glfwSetWindowMonitor(m_Window, nullptr, m_WindowedPos[0], m_WindowedPos[1], m_WindowedSize[0], m_WindowedSize[1], GLFW_DONT_CARE);
			if (m_RestoreMaximized)
				glfwMaximizeWindow(m_Window);
		}
		m_IsFullscreen = fullscreen;
	}

	bool Window::IsMinimized() const
	{
		return glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED) == GLFW_TRUE;
	}

	void Window::SetTitle(const std::string& title)
	{
		glfwSetWindowTitle(m_Window, title.c_str());
	}

	void Window::SetCursorMode(CursorMode mode)
	{
		if (m_CursorMode == mode)
			return;

		glfwSetInputMode(m_Window, GLFW_CURSOR, ToGlfwCursorMode(mode));
		m_CursorMode = mode;

		double x, y;
		glfwGetCursorPos(m_Window, &x, &y);
		Input::SetMousePosition(x, y);
	}

	uint32_t Window::Width() const
	{
		int width, height;
		glfwGetFramebufferSize(m_Window, &width, &height);
		return static_cast<uint32_t>(width);
	}

	uint32_t Window::Height() const
	{
		int width, height;
		glfwGetFramebufferSize(m_Window, &width, &height);
		return static_cast<uint32_t>(height);
	}

	std::vector<const char*> Window::GetInstanceExtensions() const
	{
		uint32_t extensionCount = 0;
		const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
		if (!extensions)
			return {};
		return std::vector<const char*>(extensions, extensions + extensionCount);
	}
}

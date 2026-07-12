#pragma once
#include "Window.h"
#include "Input.h"
#include "RadianceEngine/Core/Log.h"

#define NOMINMAX
#define NOGDI
#include <GLFW/glfw3.h>

namespace Rdn
{
	static uint32_t s_WindowCount = 0;

	Window::Window(const WindowDescription& desc)
	{
		InitializeWindow(desc);
		SetupCallbacks();
	}

	Window::~Window()
	{
		glfwDestroyWindow(static_cast<GLFWwindow*>(m_Window));
		s_WindowCount--;

		if (s_WindowCount == 0)
		{
			glfwTerminate();
		}
	}

	void Window::InitializeWindow(const WindowDescription& desc)
	{
		if (s_WindowCount == 0)
		{
			if (!glfwInit())
			{
				LogFatal("Failed to initialize GLFW!");
				return;
			}
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
		if (!desc.Titlebar)
		{
			glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
		}

		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);

		m_IsFullscreen = desc.Fullscreen;
		m_WindowedPos[0] = static_cast<int>((mode->width - desc.Width) * 0.5f);
		m_WindowedPos[1] = static_cast<int>((mode->height - desc.Height) * 0.5f);
		m_WindowedSize[0] = desc.Width;
		m_WindowedSize[1] = desc.Height;

		int width = m_IsFullscreen ? mode->width : desc.Width;
		int height = m_IsFullscreen ? mode->height : desc.Height;

		m_Window = glfwCreateWindow(width, height, desc.Title.c_str(), m_IsFullscreen ? monitor : nullptr, nullptr);
		if (!m_Window)
		{
			LogFatal("Failed to create GLFW window!");
			return;
		}

		s_WindowCount++;
	}

	void Window::SetupCallbacks()
	{
		GLFWwindow* nativeWin = static_cast<GLFWwindow*>(m_Window);

		glfwSetWindowUserPointer(nativeWin, this);

		glfwSetKeyCallback(nativeWin, [](GLFWwindow* window, int key, int scancode, int action, int mods)
			{
				if (action == GLFW_REPEAT) return;
				Input::SetKey(static_cast<KeyCode>(key), action == GLFW_PRESS);
			});

		glfwSetMouseButtonCallback(nativeWin, [](GLFWwindow* window, int button, int action, int mods)
			{
				Input::SetKey(static_cast<KeyCode>(button), action == GLFW_PRESS);
			});

		glfwSetScrollCallback(nativeWin, [](GLFWwindow* window, double xOffset, double yOffset)
			{
				Window& win = *static_cast<Window*>(glfwGetWindowUserPointer(window));
				win.m_ScrollX = static_cast<float>(xOffset);
				win.m_ScrollY = static_cast<float>(yOffset);
			});

		glfwSetCursorPosCallback(nativeWin, [](GLFWwindow* window, double xpos, double ypos)
			{
				Window& win = *static_cast<Window*>(glfwGetWindowUserPointer(window));
				win.m_MouseX = static_cast<float>(xpos);
				win.m_MouseY = static_cast<float>(ypos);
			});
	}

	void Window::PollEvents()
	{
		m_ScrollX = 0.0f;
		m_ScrollY = 0.0f;

		Input::TransitionFrame();
		glfwPollEvents();
		Input::UpdateMouseAndScroll(m_MouseX, m_MouseY, m_ScrollX, m_ScrollY);
	}

	void Window::WaitEvents()
	{
		glfwWaitEvents();
	}

	void Window::SetClosed(bool close)
	{
		glfwSetWindowShouldClose(static_cast<GLFWwindow*>(m_Window), close);
	}

	bool Window::IsClosed() const
	{
		return glfwWindowShouldClose(static_cast<GLFWwindow*>(m_Window));
	}

	void Window::SetFullscreen(bool fullscreen)
	{
		if (m_IsFullscreen == fullscreen) return;

		GLFWwindow* nativeWin = static_cast<GLFWwindow*>(m_Window);
		if (fullscreen)
		{
			GLFWmonitor* monitor = glfwGetPrimaryMonitor();
			const GLFWvidmode* mode = glfwGetVideoMode(monitor);

			glfwGetWindowPos(nativeWin, &m_WindowedPos[0], &m_WindowedPos[1]);
			glfwGetWindowSize(nativeWin, &m_WindowedSize[0], &m_WindowedSize[1]);

			glfwSetWindowMonitor(nativeWin, monitor, 0, 0, mode->width, mode->height, GLFW_DONT_CARE);
		}
		else
		{
			glfwSetWindowMonitor(nativeWin, nullptr, m_WindowedPos[0], m_WindowedPos[1], m_WindowedSize[0], m_WindowedSize[1], GLFW_DONT_CARE);
		}

		m_IsFullscreen = fullscreen;
	}

	int Window::Width() const
	{
		int w, h;
		glfwGetWindowSize(static_cast<GLFWwindow*>(m_Window), &w, &h);
		return w;
	}

	int Window::Height() const
	{
		int w, h;
		glfwGetWindowSize(static_cast<GLFWwindow*>(m_Window), &w, &h);
		return h;
	}

	std::vector<const char*> Window::GetInstanceExtensions() const
	{
		uint32_t extensionCount = 0;
		const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
		return std::vector<const char*>(extensions, extensions + extensionCount);
	}
}
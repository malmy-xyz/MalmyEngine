#pragma once

#include "../thirdparty/glew-2.1.0/include/GL/glew.h"
#include "../thirdparty/glfw-3.2.1/include/GLFW/glfw3.h"

#include "debug.h"

class Window
{
	//functions
	public:
		Window();
		~Window();

		void Update();

	//functions
	private:


	//variables
	public:
		GLFWwindow* m_Window;


	//variables
	private:
		int m_Width = 1366;
		int m_Height = 768;


};


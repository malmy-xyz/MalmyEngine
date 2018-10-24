#pragma once

#include "../thirdparty/glew-2.1.0/include/GL/glew.h"
#include "../thirdparty/glfw-3.2.1/include/GLFW/glfw3.h"

#include "debug.h"

class Window
{
	//functions
	public:
		Window(int width, int height);
		~Window();

		void Update();

	//functions
	private:


	//variables
	public:
		GLFWwindow* m_Window;


	//variables
	private:



};


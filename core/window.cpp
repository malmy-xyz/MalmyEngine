#include "window.h"



Window::Window()
{
	
	/* Initialize the library */
	if (!glfwInit())
		Debug::LogError("glfwInit ");



	/* Create a windowed mode window and its OpenGL context */
	m_Window = glfwCreateWindow(m_Width, m_Height, "Malmy Engine v0.0.0 ", NULL, NULL);
	if (!m_Window)
	{
		
		glfwTerminate();
		Debug::LogError("glfwTerminate");
	}

	/* Make the window's context current */
	glfwMakeContextCurrent(m_Window);

	glfwSwapInterval(1);

	if (glewInit() != GLEW_OK) {
		Debug::LogError("glewInit Error");
	}

	// burayi duzelt
	//Debug::LogInfo(glGetString(GL_VERSION));
	//std::cout << glGetString(GL_VERSION) << std::endl;

	
}


Window::~Window()
{
	glfwTerminate();
}

void Window::Update() 
{
	if (!glfwWindowShouldClose(m_Window))
	{
		//
		Debug::Log("Window::Update ");
	}
}
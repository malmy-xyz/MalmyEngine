#include "testRender.h"

#include "../../thirdparty/glfw-3.2.1/include/GLFW/glfw3.h"
#include "../../thirdparty/glew-2.1.0/include/GL/glew.h"


TestRender::TestRender()
{
	/*Window* m_Window = new Window();
	Mesh monkey("./res/monkey3.obj");
	Shader shader("./res/basicShader");
	Texture texture("./res/bricks.jpg");
	Transform transform;
	Camera camera(glm::vec3(0.0f, 0.0f, -5.0f), 70.0f, 800 / 600, 0.1f, 100.0f);*/


	/* Initialize the library */
	if (!glfwInit())
		//return -1;

	/* Create a windowed mode window and its OpenGL context */
	window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);

	if (!window)
	{
		glfwTerminate();
		//return -1;
	}

	/* Make the window's context current */
	glfwMakeContextCurrent(window);

	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window))
	{
		/* Render here */
		glClear(GL_COLOR_BUFFER_BIT);

		/* Swap front and back buffers */
		glfwSwapBuffers(window);

		/* Poll for and process events */
		glfwPollEvents();
	}

	glfwTerminate();

}


TestRender::~TestRender()
{
}

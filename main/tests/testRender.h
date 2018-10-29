#pragma once

#include "../../core/window.h"

#include "../../core/rendering/mesh.h"
#include "../../core/rendering/shader.h"
#include "../../core/rendering/texture.h"

#include "../../components/camera.h"
#include "../../components/transform.h"


class TestRender
{
public:
	TestRender();
	~TestRender();

	GLFWwindow* window;
};


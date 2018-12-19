#pragma once
#include "../rendering/mesh.h"
#include "../rendering/shader.h"
#include "../core/transform.h"
#include "../rendering/camera.h"
#include "../rendering/lighting.h"
#include "../core/GameObject.h"
#include "../components/meshRenderer.h"
#include "../rendering/window.h"
#include "../core/Engine.h"
#include "../core/scene.h"

#include "../core/debug.h"

//SDL makrosu burasi
#undef main

#include "../editor/editorCameraMove.h"


class TestScene : public Scene
{
public:
	TestScene() {}

	virtual void Init(const Window& window);
protected:
private:
	TestScene(const TestScene& other) {}
	void operator=(const TestScene& other) {}
};


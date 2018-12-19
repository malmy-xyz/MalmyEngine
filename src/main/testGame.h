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
#include "../core/game.h"

#include "../core/debug.h"

//SDL makrosu burasi
#undef main

#include "../editor/editorCameraMove.h"


class TestGame : public Game
{
public:
	TestGame() {}

	virtual void Init(const Window& window);
protected:
private:
	TestGame(const TestGame& other) {}
	void operator=(const TestGame& other) {}
};


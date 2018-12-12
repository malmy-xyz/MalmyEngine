#ifndef __TESTGAME_H_INCLUDED
#define __TESTGAME_H_INCLUDED

//This header is just a convinient way to include all necessary files to use the engine.

#include "../rendering/mesh.h"
#include "../rendering/shader.h"
#include "../core/transform.h"
#include "../rendering/camera.h"
#include "../rendering/lighting.h"
#include "../core/entity.h"
#include "../components/meshRenderer.h"
#include "../rendering/window.h"
#include "../core/coreEngine.h"
#include "../core/game.h"

#include "../core/debug.h"

//SDL makrosu burasi
#undef main

#endif // TESTGAME_H_INCLUDED

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


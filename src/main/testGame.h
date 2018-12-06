
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

//SDL2 defines a main macro, which can prevent certain compilers from finding the main function.
#undef main

#endif // TESTGAME_H_INCLUDED

#include "../components/freeLook.h"
#include "../components/freeMove.h"
#include "../components/physicsEngineComponent.h"
#include "../components/physicsObjectComponent.h"
#include "../physics/boundingSphere.h"

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


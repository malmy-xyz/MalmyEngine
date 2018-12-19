#include "scene.h"

#include "../rendering/renderEngine.h"

#include <iostream>

void Scene::ProcessInput(const Input& input, float delta)
{
	m_inputTimer.StartInvocation();
	m_root.ProcessInputAll(input, delta);
	m_inputTimer.StopInvocation();
}

void Scene::Update(float delta)
{
	m_updateTimer.StartInvocation();
	m_root.UpdateAll(delta);
	m_updateTimer.StopInvocation();
}

void Scene::Render(renderEngine* renderEngine)
{
	renderEngine->Render(m_root);
}

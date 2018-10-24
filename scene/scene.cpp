#include "scene.h"



Scene::Scene()
{
	Debug::Log("Scene::Scene ");
}


Scene::~Scene()
{

}

void Scene::AddGameObject(GameObject* m_Object)
{


	m_GameObjects.push_back(m_Object);
	
	
}

void Scene::Update()
{
	Debug::Log("Scene::Update ");

	for (unsigned int i = 0; i < m_GameObjects.size(); i++)
	{
		m_GameObjects[i]->Update();
	}
	
}

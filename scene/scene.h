#pragma once

#include <vector>

#include "../core/gameObject.h"

#include "../core/debug.h"

class Scene
{
	//functions
	public:
		Scene();
		~Scene();

		void AddGameObject(GameObject* m_Object);
		void Update();


	//functions
	private:

	//variables
	public:
		std::vector<GameObject*> m_GameObjects;

	//variables
	private:


};

/*

burda shne bilgileri olucak ama bunlari .mscene dosayinda okucaz
simdilik manuel olustur

*/

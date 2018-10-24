#pragma once

#include <iostream>
#include <vector>

#include "../components/transform.h"
#include "../components/component.h"


#include "debug.h"

class GameObject
{
	//functions
	public:
		GameObject();
		~GameObject();

		void AddComponent(Component* m_Component);

		void Update();

	//functions
	private:

	//variables
	public:
		std::string m_Icon;
		std::string m_Name;	
		bool m_IsActive;
		std::vector<Component*> m_Components;

	//variables
	private:
		
};


/*

m_ObjectHideFlags: 0
m_PrefabParentObject: {fileID: 0}
m_PrefabInternal: {fileID: 0}
serializedVersion: 5
m_Component:
- component: {fileID: 132648833}
- component: {fileID: 132648832}
m_Layer: 0
m_Name: GameObject
m_TagString: Untagged
m_Icon: {fileID: 0}
m_NavMeshLayer: 0
m_StaticEditorFlags: 0
m_IsActive: 1

*/
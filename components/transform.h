#pragma once

#include <iostream>
#include <vector>

#include "component.h"

#include "../maths/vector3.h"
#include "../maths/quaternion.h"


class Transform : public Component
{
	//functions
	public:
		Transform();

		Transform(Vector3 localPosition, Quaternion localRotation, Vector3 localScale);
		
		void SetParent(Transform* parent);
		void AddChild(Transform* child);

		virtual ~Transform();

		//

	//functions
	private:

	//variables
	public:

		//get() and set() 
		Vector3 m_Position;
		Quaternion m_Rotation;
		Vector3 m_Scale;

		Transform* m_Parent;
		std::vector<Transform*> m_Childrens;

	//variables
	private:


};

/*

m_ObjectHideFlags: 0
m_PrefabParentObject: {fileID: 0}
m_PrefabInternal: {fileID: 0}
m_GameObject: {fileID: 132648831}
m_LocalRotation: {x: 0, y: 0, z: 0, w: 1}
m_LocalPosition: {x: 0, y: 0, z: 0}
m_LocalScale: {x: 1, y: 1, z: 1}
m_Children: []
m_Father: {fileID: 0}
m_RootOrder: 2
m_LocalEulerAnglesHint: {x: 0, y: 0, z: 0}

*/


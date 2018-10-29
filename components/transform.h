#pragma once

#include <iostream>
#include <vector>

#include "component.h"
#include "camera.h"

#include "../thirdparty/glm-0.9.9.2/glm/glm.hpp"
#include "../thirdparty/glm-0.9.9.2/glm/gtx/transform.hpp"


class Transform : public Component
{
public:
	Transform(const glm::vec3& pos = glm::vec3(), const glm::vec3& rot = glm::vec3(), const glm::vec3& scale = glm::vec3(1.0f, 1.0f, 1.0f))
	{
		m_Position = pos;
		m_Rotation = rot;
		m_Scale = scale;

	}

	inline glm::mat4 GetModel() const
	{
		glm::mat4 posMat = glm::translate(m_Position);
		glm::mat4 scaleMat = glm::scale(m_Scale);
		glm::mat4 rotX = glm::rotate(m_Rotation.x, glm::vec3(1.0, 0.0, 0.0));
		glm::mat4 rotY = glm::rotate(m_Rotation.y, glm::vec3(0.0, 1.0, 0.0));
		glm::mat4 rotZ = glm::rotate(m_Rotation.z, glm::vec3(0.0, 0.0, 1.0));
		glm::mat4 rotMat = rotX * rotY * rotZ;

		return posMat * rotMat * scaleMat;
	}

	inline glm::mat4 GetMVP(const Camera& camera) const
	{
		glm::mat4 VP = camera.GetViewProjection();
		glm::mat4 M = GetModel();

		return VP * M;//camera.GetViewProjection() * GetModel();
	}

	inline glm::vec3* GetPos() { return &m_Position; }
	inline glm::vec3* GetRot() { return &m_Rotation; }
	inline glm::vec3* GetScale() { return &m_Scale; }

	inline void SetPos(glm::vec3& pos) { this->m_Position = pos; }
	inline void SetRot(glm::vec3& rot) { this->m_Rotation = rot; }
	inline void SetScale(glm::vec3& scale) { this->m_Scale = scale; }

	inline void AddChild(Transform* m_Child) { m_Childs.push_back(m_Child); }

protected:
private:
	glm::vec3 m_Position;
	glm::vec3 m_Rotation;
	glm::vec3 m_Scale;

	Transform* m_Parent;
	std::vector<Transform*> m_Childs;



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


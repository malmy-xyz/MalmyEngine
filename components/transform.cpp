#include "transform.h"



Transform::Transform()
{
	//const
	Debug::Log("Transform::Transform ");

}

Transform::~Transform()
{
	//desc
}

Transform::Transform(Vector3 m_LocalPosition, Quaternion m_LocalRotation, Vector3 m_LocalScale)
{
	Debug::Log("Transform::Transform ");
}

void Transform::SetParent(Transform* parent)
{
	m_Parent = parent;
}

void Transform::AddChild(Transform * child)
{
	m_Childrens.push_back(child);
	child->SetParent(this);
}

#include "gameObject.h"



GameObject::GameObject()
{
	//const
	Debug::Log("GameObject::GameObject ");


	m_Name = "New Game Object";
}


GameObject::~GameObject()
{
	//desct
}

void GameObject::AddComponent(Component* m_Component)
{
	//burda return ile geri donduredebiliriz

	Debug::Log("GameObject::AddComponent ");

	m_Components.push_back(m_Component);

}

void GameObject::Update()
{
	Debug::Log("GameObject::Update ");

	for (unsigned int i = 0; i < m_Components.size(); i++)
	{
		m_Components[i]->Update();
	}

}

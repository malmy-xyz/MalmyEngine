#include "GameObject.h"
#include "../components/component.h"
#include "Engine.h"

//GameObject //UNity de object iste

GameObject::~GameObject()
{
	for(unsigned int i = 0; i < m_components.size(); i++)
	{
		if(m_components[i])
		{	
			delete m_components[i];
		}
	}
	
	for(unsigned int i = 0; i < m_children.size(); i++)
	{
		if(m_children[i]) 
		{
			delete m_children[i];
		}
	}
}

//child ekleme
GameObject* GameObject::AddChild(GameObject* child)
{
	m_children.push_back(child); 
	child->GetTransform()->SetParent(&m_transform);
	child->SetEngine(m_Engine);
	return this;
}

//componet ekelem
GameObject* GameObject::AddComponent(Component* component)
{
	m_components.push_back(component);
	component->SetParent(this);
	return this;
}

//input vs algilama
void GameObject::ProcessInputAll(const Input& input, float delta)
{
	ProcessInput(input, delta);

	for(unsigned int i = 0; i < m_children.size(); i++)
	{
		m_children[i]->ProcessInputAll(input, delta);
	}
}

//cocuklari ile birlikte Update et
//corengien cagiriyo bunu
//alt cocuklarina gore
void GameObject::UpdateAll(float delta)
{
	Update(delta);

	for(unsigned int i = 0; i < m_children.size(); i++)
	{
		m_children[i]->UpdateAll(delta);
	}
}


//cocuklari ile birlikte Render
//corengien cagiriyo bunu
void GameObject::RenderAll(const Shader& shader, const renderEngine& renderEngine, const Camera& camera) const
{
	Render(shader, renderEngine, camera);

	for(unsigned int i = 0; i < m_children.size(); i++)
	{
		m_children[i]->RenderAll(shader, renderEngine, camera);
	}
}

//input al
void GameObject::ProcessInput(const Input& input, float delta)
{
	m_transform.Update();

	for(unsigned int i = 0; i < m_components.size(); i++)
	{
		m_components[i]->ProcessInput(input, delta);
	}
}

//update butun componeteleri
void GameObject::Update(float delta)
{
	for(unsigned int i = 0; i < m_components.size(); i++)
	{
		m_components[i]->Update(delta);
	}
}

//butun componentleri render ey
void GameObject::Render(const Shader& shader, const renderEngine& renderEngine, const Camera& camera) const
{
	for(unsigned int i = 0; i < m_components.size(); i++)
	{
		m_components[i]->Render(shader, renderEngine, camera);
	}
}

//engien yi ayarala
void GameObject::SetEngine(Engine* engine)
{
	if(m_Engine != engine)
	{
		m_Engine = engine;
		
		for(unsigned int i = 0; i < m_components.size(); i++)
		{
			m_components[i]->AddToEngine(engine);
		}

		for(unsigned int i = 0; i < m_children.size(); i++)
		{
			m_children[i]->SetEngine(engine);
		}
	}
}

//coculara erisme kismi burasi
std::vector<GameObject*> GameObject::GetAllAttached()
{
	std::vector<GameObject*> result;
	
	for(unsigned int i = 0; i < m_children.size(); i++)
	{
		std::vector<GameObject*> childObjects = m_children[i]->GetAllAttached();
		result.insert(result.end(), childObjects.begin(), childObjects.end());
	}
	
	result.push_back(this);
	return result;
}
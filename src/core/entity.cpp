#include "entity.h"
#include "gameObject.h"
#include "coreEngine.h"

//entity //UNity de object iste

Entity::~Entity()
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
Entity* Entity::AddChild(Entity* child)
{
	m_children.push_back(child); 
	child->GetTransform()->SetParent(&m_transform);
	child->SetEngine(m_coreEngine);
	return this;
}

//componet ekelem
Entity* Entity::AddComponent(GameObject* component)
{
	m_components.push_back(component);
	component->SetParent(this);
	return this;
}

//input vs algilama
void Entity::ProcessInputAll(const Input& input, float delta)
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
void Entity::UpdateAll(float delta)
{
	Update(delta);

	for(unsigned int i = 0; i < m_children.size(); i++)
	{
		m_children[i]->UpdateAll(delta);
	}
}


//cocuklari ile birlikte Render
//corengien cagiriyo bunu
void Entity::RenderAll(const Shader& shader, const RenderingEngine& renderingEngine, const Camera& camera) const
{
	Render(shader, renderingEngine, camera);

	for(unsigned int i = 0; i < m_children.size(); i++)
	{
		m_children[i]->RenderAll(shader, renderingEngine, camera);
	}
}

//input al
void Entity::ProcessInput(const Input& input, float delta)
{
	m_transform.Update();

	for(unsigned int i = 0; i < m_components.size(); i++)
	{
		m_components[i]->ProcessInput(input, delta);
	}
}

//update butun componeteleri
void Entity::Update(float delta)
{
	for(unsigned int i = 0; i < m_components.size(); i++)
	{
		m_components[i]->Update(delta);
	}
}

//butun componentleri render ey
void Entity::Render(const Shader& shader, const RenderingEngine& renderingEngine, const Camera& camera) const
{
	for(unsigned int i = 0; i < m_components.size(); i++)
	{
		m_components[i]->Render(shader, renderingEngine, camera);
	}
}

//engien yi ayarala
void Entity::SetEngine(CoreEngine* engine)
{
	if(m_coreEngine != engine)
	{
		m_coreEngine = engine;
		
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
std::vector<Entity*> Entity::GetAllAttached()
{
	std::vector<Entity*> result;
	
	for(unsigned int i = 0; i < m_children.size(); i++)
	{
		std::vector<Entity*> childObjects = m_children[i]->GetAllAttached();
		result.insert(result.end(), childObjects.begin(), childObjects.end());
	}
	
	result.push_back(this);
	return result;
}

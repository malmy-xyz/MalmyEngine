#ifndef GameObject_H_INCLUDED
#define GameObject_H_INCLUDED

#include "transform.h"
#include "entity.h"
#include "input.h"
class RenderingEngine;
class Shader;

//Game object gibi bisey bu 
class GameObject
{
public:
	GameObject() :
		m_parent(0) {}
	virtual ~GameObject() {}

	virtual void ProcessInput(const Input& input, float delta) {}
	virtual void Update(float delta) {}
	virtual void Render(const Shader& shader, const RenderingEngine& renderingEngine, const Camera& camera) const {}
	
	virtual void AddToEngine(CoreEngine* engine) const { }
	
	inline Transform* GetTransform()             { return m_parent->GetTransform(); }
	inline const Transform& GetTransform() const { return *m_parent->GetTransform(); }
	
	virtual void SetParent(Entity* parent) { m_parent = parent; }
private:
	Entity* m_parent;
	
	GameObject(const GameObject& other) {}
	void operator=(const GameObject& other) {}
};

#endif // GAMECOMPONENT_H_INCLUDED

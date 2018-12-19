#pragma once
#include "../core/transform.h"
#include "../core/GameObject.h"
#include "../core/input.h"

#include <iostream>

class RenderingEngine;
class Shader;

class Component
{
public:

	Component() :
		m_parent(0) {}

	virtual ~Component() {}


	virtual void ProcessInput(const Input& input, float delta) {}
	virtual void Update(float delta) {
	
		//std::cout << "ec " << std::endl;
	}

	virtual void Render(const Shader& shader, const RenderingEngine& renderingEngine, const Camera& camera) const {}

	virtual void AddToEngine(Engine* engine) const { }

	inline Transform* GetTransform() { return m_parent->GetTransform(); }
	inline const Transform& GetTransform() const { return *m_parent->GetTransform(); }

	virtual void SetParent(GameObject* parent) { m_parent = parent; }

private:

	GameObject* m_parent;
	Component(const Component& other) {}
	void operator=(const Component& other) {}
};
#pragma once
#include <vector>
#include "transform.h"
#include "input.h"

class Camera;
class Engine;
class Component;
class Shader;
class renderEngine;

class GameObject
{
public:
	GameObject(const Vector3f& pos = Vector3f(0,0,0), const Quaternion& rot = Quaternion(0,0,0,1), float scale = 1.0f) : 
		m_transform(pos, rot, scale),
		m_Engine(0) {}
		
	virtual ~GameObject();
	
	GameObject* AddChild(GameObject* child);
	GameObject* AddComponent(Component* component);
	
	void ProcessInputAll(const Input& input, float delta);
	void UpdateAll(float delta);
	void RenderAll(const Shader& shader, const renderEngine& renderEngine, const Camera& camera) const;
	
	std::vector<GameObject*> GetAllAttached();
	
	inline Transform* GetTransform() { return &m_transform; }
	void SetEngine(Engine* engine);
protected:
private:
	std::vector<GameObject*>        m_children;
	std::vector<Component*>			m_components;
	Transform						m_transform;
	Engine*							m_Engine;

	void ProcessInput(const Input& input, float delta);
	void Update(float delta);
	void Render(const Shader& shader, const renderEngine& renderEngine, const Camera& camera) const;
	
	GameObject(const GameObject& other) {}
	void operator=(const GameObject& other) {}
};


/* : Object

public GameObject();
public GameObject(string name);
public GameObject(string name, params Type[] components);

public int layer { get; set; }
public GameObject gameObject { get; }
public Transform transform { get; }

public T AddComponent<T>() where T : Component;
public Component AddComponent(Type componentType);
public Component AddComponent(string className);

public T GetComponent<T>();
public Component GetComponent(Type type);
public Component GetComponent(string type);

public T[] GetComponents<T>();
public void GetComponents(Type type, List<Component> results);
public void GetComponents<T>(List<T> results);

void Input(float delta);		//input
void FixedUpdate(float delta);	//physic
void Update(float delta);		//render

public GameObject parent { get; set; }
public GameObject AddChild(GameObject* child);
public GameObject GetChild(GameObject* child);
std::vector<GameObject*>        m_children;

*/
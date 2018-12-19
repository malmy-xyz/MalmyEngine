#pragma once
#include "gameObject.h"
#include "engine.h"
#include "profiling.h"

class Scene
{
public:
	Scene() {}
	virtual ~Scene() {}

	virtual void Init(const Window& window) {}
	void ProcessInput(const Input& input, float delta);
	void Update(float delta);
	void Render(renderEngine* renderEngine);
	
	inline double DisplayInputTime(double dividend) { return m_inputTimer.DisplayAndReset("Input Time: ", dividend); }
	inline double DisplayUpdateTime(double dividend) { return m_updateTimer.DisplayAndReset("Update Time: ", dividend); }
	
	inline void SetEngine(Engine* engine) { m_root.SetEngine(engine); }
protected:
	void AddToScene(GameObject* child) { m_root.AddChild(child); }
private:
	Scene(Scene& game) {}
	void operator=(Scene& game) {}
	
	ProfileTimer m_updateTimer;
	ProfileTimer m_inputTimer;
	GameObject       m_root;

};

#pragma once
#include "GameObject.h"
#include "Engine.h"
#include "profiling.h"

class Game
{
public:
	Game() {}
	virtual ~Game() {}

	virtual void Init(const Window& window) {}
	void ProcessInput(const Input& input, float delta);
	void Update(float delta);
	void Render(RenderingEngine* renderingEngine);
	
	inline double DisplayInputTime(double dividend) { return m_inputTimer.DisplayAndReset("Input Time: ", dividend); }
	inline double DisplayUpdateTime(double dividend) { return m_updateTimer.DisplayAndReset("Update Time: ", dividend); }
	
	inline void SetEngine(Engine* engine) { m_root.SetEngine(engine); }
protected:
	void AddToScene(GameObject* child) { m_root.AddChild(child); }
private:
	Game(Game& game) {}
	void operator=(Game& game) {}
	
	ProfileTimer m_updateTimer;
	ProfileTimer m_inputTimer;
	GameObject       m_root;
};

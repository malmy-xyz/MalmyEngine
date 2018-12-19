#pragma once
#include "../components/component.h"

class MalmyScript : public Component
{
public:
	virtual void ProcessInput(const Input& input, float delta) {}
	virtual void Update(float delta) {
	
		//std::cout << " ms " << delta << std::endl;
	}
	virtual void Render(const Shader& shader, const RenderingEngine& renderingEngine, const Camera& camera) const {}

protected:
private:

};

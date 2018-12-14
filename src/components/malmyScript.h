#pragma once
#include "../entityComponent.h"

class MalmyScript : public EntityComponent
{
public:
	virtual void ProcessInput(const Input& input, float delta) {}
	virtual void Update(float delta) {}
	virtual void Render(const Shader& shader, const RenderingEngine& renderingEngine, const Camera& camera) const {}

protected:
private:

};

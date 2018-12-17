#pragma once
#include "../../components/malmyScript.h"

class DemoRot : public MalmyScript
{
public:

	DemoRot(int carp) :
		m_Carpan(carp)
	{ }

	virtual void ProcessInput(const Input& input, float delta) {}
	virtual void Update(float delta) {
		//std::cout << "script " << std::endl;
		//delta kismi time delta time ile alincak
		//butun updateler degiscek

		GetTransform()->Rotate(Vector3f(0, 1, 0), ToRadians(m_Carpan * delta));
	}
	virtual void Render(const Shader& shader, const RenderingEngine& renderingEngine, const Camera& camera) const {}

protected:
private:
	float m_Carpan;

};

#pragma once
#include "../maths/math3d.h"
#include "../components/component.h"

class EditorCameraMove : public Component
{
public:

	EditorCameraMove(/*const Vector2f& windowCenter*/):
		m_sensitivity(0.5f),
		m_speed(10.0f)
	{}


	virtual void ProcessInput(const Input& input, float delta);
protected:
private:
	float    m_sensitivity;
	Vector2f m_mousePos;

	void Move(const Vector3f& direction, float amt);
	float m_speed;


};



#ifndef EDITORCAMERA_H
#define EDITORCAMERA_H


#include "../maths/math3d.h"
#include "../core/gameObject.h"

class EditorCameraMove : public GameObject
{
public:

	EditorCameraMove(const Vector2f& windowCenter):
		m_sensitivity(0.5f),
		m_unlockMouseKey(Input::KEY_ESCAPE),
		m_mouseLocked(false),
		m_windowCenter(windowCenter),

		m_speed(10.0f),
		m_forwardKey(Input::KEY_W),
		m_backKey(Input::KEY_S),
		m_leftKey(Input::KEY_A),
		m_rightKey(Input::KEY_D)

	{}


	virtual void ProcessInput(const Input& input, float delta);
protected:
private:
	float    m_sensitivity;
	int      m_unlockMouseKey;
	bool     m_mouseLocked;
	Vector2f m_windowCenter;

	void Move(const Vector3f& direction, float amt);
	float m_speed;
	int m_forwardKey;
	int m_backKey;
	int m_leftKey;
	int m_rightKey;

};

#endif //EDITORCAMERA_H



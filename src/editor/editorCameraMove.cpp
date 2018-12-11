#include "editorCameraMove.h"

void EditorCameraMove::ProcessInput(const Input& input, float delta) {
	//move
	float movAmt = m_speed * delta;

	if (input.GetKey(m_forwardKey))
		Move(GetTransform()->GetRot()->GetForward(), movAmt);
	if (input.GetKey(m_backKey))
		Move(GetTransform()->GetRot()->GetBack(), movAmt);
	if (input.GetKey(m_leftKey))
		Move(GetTransform()->GetRot()->GetLeft(), movAmt);
	if (input.GetKey(m_rightKey))
		Move(GetTransform()->GetRot()->GetRight(), movAmt);

	//look
	if (input.GetKey(m_unlockMouseKey))
	{
		input.SetCursor(true);
		m_mouseLocked = false;
	}

	if (m_mouseLocked)
	{
		Vector2f deltaPos = input.GetMousePosition() - m_windowCenter;

		bool rotY = deltaPos.GetX() != 0;
		bool rotX = deltaPos.GetY() != 0;

		if (rotY)
		{
			GetTransform()->Rotate(Vector3f(0, 1, 0), ToRadians(deltaPos.GetX() * m_sensitivity));
		}
		if (rotX)
		{
			GetTransform()->Rotate(GetTransform()->GetRot()->GetRight(), ToRadians(deltaPos.GetY() * m_sensitivity));
		}

		if (rotY || rotX)
		{
			input.SetMousePosition(m_windowCenter);
		}
	}

	if (input.GetMouseDown(Input::MOUSE_LEFT_BUTTON))
	{
		input.SetCursor(false);
		input.SetMousePosition(m_windowCenter);
		m_mouseLocked = true;
	}


}

void EditorCameraMove::Move(const Vector3f& direction, float amt)
{
	GetTransform()->SetPos(*GetTransform()->GetPos() + (direction * amt));
}



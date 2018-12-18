#include "editorCameraMove.h"

void EditorCameraMove::ProcessInput(const Input& input, float delta) {
	//move
	float movAmt = m_speed * delta;

	if (input.GetKey(Input::KEY_W)) 
	{
		Move(GetTransform()->GetRot()->GetForward(), movAmt);
	}

	if (input.GetKey(Input::KEY_S)) 
	{
		Move(GetTransform()->GetRot()->GetBack(), movAmt);
	}

	if (input.GetKey(Input::KEY_A)) 
	{
		Move(GetTransform()->GetRot()->GetLeft(), movAmt);
	}

	if (input.GetKey(Input::KEY_D)) 
	{
		Move(GetTransform()->GetRot()->GetRight(), movAmt);
	}

	//
	if (!input.GetKey(Input::KEY_LCTRL) && input.GetKeyDown(Input::KEY_KP_1))
	{
		std::cout << "front " << std::endl;

		GetTransform()->SetPos(Vector3f(0, 0, -10));
		GetTransform()->SetRot(Quaternion(Vector3f(0, 1, 0), ToRadians(0)));


	}
	if (input.GetKey(Input::KEY_LCTRL) && input.GetKeyDown(Input::KEY_KP_1))
	{
		std::cout << "back " << std::endl;

		GetTransform()->SetPos(Vector3f(0, 0, 10));
		GetTransform()->SetRot(Quaternion(Vector3f(0, 1, 0), ToRadians(180)));
		
	}
	if (!input.GetKey(Input::KEY_LCTRL) && input.GetKeyDown(Input::KEY_KP_3))
	{
		std::cout << "right " << std::endl;

		GetTransform()->SetPos(Vector3f(10, 0, 0));
		GetTransform()->SetRot(Quaternion(Vector3f(0, 1, 0), ToRadians(-90)));

	}
	if (input.GetKey(Input::KEY_LCTRL) && input.GetKeyDown(Input::KEY_KP_3))
	{
		std::cout << "left " << std::endl;

		GetTransform()->SetPos(Vector3f(-10, 0, 0));
		GetTransform()->SetRot(Quaternion(Vector3f(0, 1, 0), ToRadians(90)));

	}
	if (!input.GetKey(Input::KEY_LCTRL) && input.GetKeyDown(Input::KEY_KP_7))
	{
		std::cout << "top " << std::endl;

		GetTransform()->SetPos(Vector3f(0, 10, 0));
		GetTransform()->SetRot(Quaternion(Vector3f(1, 0, 0), ToRadians(90)));

	}
	if (input.GetKey(Input::KEY_LCTRL) && input.GetKeyDown(Input::KEY_KP_7))
	{
		std::cout << "bottom " << std::endl;

		GetTransform()->SetPos(Vector3f(0, -10, 0));
		GetTransform()->SetRot(Quaternion(Vector3f(1, 0, 0), ToRadians(-90)));

	}
	if (input.GetKeyDown(Input::KEY_KP_9))
	{
		std::cout << "dondur " << std::endl;
		//bunu sonra yap
	}
	if (input.GetKeyDown(Input::KEY_KP_5))
	{
		std::cout << "ortho / pers" << std::endl;
		//bunuda sonra yap
	}


	//Mouse hareketleri
	if (input.GetMouseDown(Input::MOUSE_RIGHT_BUTTON))
	{
		m_mousePos = input.GetMousePosition();
	}
	if (input.GetMouse(Input::MOUSE_RIGHT_BUTTON))
	{
		input.SetCursor(false);

		Vector2f deltaPos = input.GetMousePosition() - m_mousePos;

		GetTransform()->Rotate(Vector3f(0, 1, 0), ToRadians(deltaPos.GetX() * m_sensitivity));
		GetTransform()->Rotate(GetTransform()->GetRot()->GetRight(), ToRadians(deltaPos.GetY() * m_sensitivity));

		m_mousePos = input.GetMousePosition();

		
	}
	else {
		//std::cout << "sag BASMIYO" << std::endl;
		//cursor kaybaoluyo ileride cusorun sekli degisebilir
		//havali gozukuyo ole yapinca :D
		input.SetCursor(true);

	}

	if (input.GetMouseDown(Input::MOUSE_LEFT_BUTTON))
	{
		//secicek faln ksimi
		//tabi duruma gore
		//pan transform rotate scale vs

	}




}

void EditorCameraMove::Move(const Vector3f& direction, float amt)
{
	GetTransform()->SetPos(*GetTransform()->GetPos() + (direction * amt));
}



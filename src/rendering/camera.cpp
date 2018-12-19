#include "camera.h"
#include "renderingEngine.h"

#include "../core/Engine.h"

Matrix4f Camera::GetViewProjection() const
{
	
	Matrix4f cameraRotation = GetTransform().GetTransformedRot().Conjugate().ToRotationMatrix();
	Matrix4f cameraTranslation;

	//camera transfrmunun harektleri
	cameraTranslation.InitTranslation(GetTransform().GetTransformedPos() * -1);

	return m_projection * cameraRotation * cameraTranslation;
}


void CameraComponent::AddToEngine(Engine* engine) const
{
	//multi kamera destegi yok
	engine->GetRenderingEngine()->SetMainCamera(m_camera);
}

void CameraComponent::SetParent(GameObject* parent)
{
	Component::SetParent(parent);

	//cameranin pzosyonunu yatla
	m_camera.SetTransform(GetTransform());
}
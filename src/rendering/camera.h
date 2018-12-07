#ifndef CAMERA_H
#define CAMERA_H

#include "../maths/math3d.h"
#include "../core/entityComponent.h"

//trasnform pers/ortho 
class Camera
{
public:
	//projection matris
	Camera(const Matrix4f& projection, Transform* transform) :
		m_projection(projection),
		m_transform(transform) {}

	inline Transform* GetTransform() { return m_transform; }
	inline const Transform& GetTransform() const { return *m_transform; }

	//- sol alt + sag ust
	Matrix4f GetViewProjection()           const;

	inline void SetProjection(const Matrix4f& projection) { m_projection = projection; }
	inline void SetTransform(Transform* transform) { m_transform = transform; }
protected:
private:
	Matrix4f   m_projection; //pres/ortho
	Transform* m_transform;  //trasnformu
};

//camera componet bunu ayir ve componts altina koy
class CameraComponent : public EntityComponent
{
public:
//trasnform null
	CameraComponent(const Matrix4f& projection) :
		m_camera(projection, 0) {}

	virtual void AddToEngine(CoreEngine* engine) const;

	inline Matrix4f GetViewProjection() const { return m_camera.GetViewProjection(); }

	inline void SetProjection(const Matrix4f& projection) { m_camera.SetProjection(projection); }
	virtual void SetParent(Entity* parent);
protected:
private:
	Camera m_camera; //kullailan camera
};

#endif
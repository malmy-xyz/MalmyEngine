#pragma once
#include "../maths/math3d.h"

//aslinda bu bir componet ama cok falza ozelligi var 
//biraz daha toplayinca compnete ata 
//kullaici scrip yarken bunu kullnicak cunku

class Transform
{
public:
	Transform(const Vector3f& pos = Vector3f(0,0,0), const Quaternion& rot = Quaternion(0,0,0,1), float scale = 1.0f) :
		m_pos(pos),
		m_rot(rot),
		m_scale(scale),
		m_parent(0),
		m_parentMatrix(Matrix4f().InitIdGameObject()),
		m_initializedOldStuff(false) {}

	Matrix4f GetTransformation() const;
	bool HasChanged();
	void Update();
	void Rotate(const Vector3f& axis, float angle);
	void Rotate(const Quaternion& rotation);
	void LookAt(const Vector3f& point, const Vector3f& up);
	
	Quaternion GetLookAtRotation(const Vector3f& point, const Vector3f& up) 
	{ 
		return Quaternion(Matrix4f().InitRotationFromDirection((point - m_pos).Normalized(), up)); 
	}
	
	inline Vector3f* GetPos()                   { return &m_pos; }
	inline const Vector3f& GetPos()       const { return m_pos; }
	inline Quaternion* GetRot()                 { return &m_rot; }
	inline const Quaternion& GetRot()     const { return m_rot; }
	inline float GetScale()               const { return m_scale; }
	inline Vector3f GetTransformedPos()   const { return Vector3f(GetParentMatrix().Transform(m_pos)); }
	Quaternion GetTransformedRot()        const;

	inline void SetPos(const Vector3f& pos) { m_pos = pos; }
	inline void SetRot(const Quaternion& rot) { m_rot = rot; }
	inline void SetScale(float scale) { m_scale = scale; }
	inline void SetParent(Transform* parent) { m_parent = parent; }
protected:
private:
	const Matrix4f& GetParentMatrix() const;

	Vector3f m_pos;
	Quaternion m_rot;
	float m_scale;
	
	Transform* m_parent;
	mutable Matrix4f m_parentMatrix;
	
	mutable Vector3f m_oldPos;
	mutable Quaternion m_oldRot;
	mutable float m_oldScale;
	mutable bool m_initializedOldStuff;
};

/* : Component

public Vector3 position { get; set; }
public Quaternion rotation { get; set; }
public Vector3 scale { get; set; }

public bool hasChanged { get; set; }

public void LookAt(Transform target);
public void LookAt(Vector3 worldPosition);

public void Rotate(Vector3 axis, float angle);
public void Translate(Vector3 translation);
public void Scale(Vector3 scale);


void Update(float delta);		//render

*/

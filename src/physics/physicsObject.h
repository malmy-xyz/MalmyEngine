#ifndef PHYSICS_OBJECT_INCLUDED_H
#define PHYSICS_OBJECT_INCLUDED_H

#include "../core/math3d.h"
#include "collider.h"

/**
 * The PhysicsObject class represents an object that can be used in a physics
 * engine.
 */
class PhysicsObject
{
public:
	/**
	 * Creates a PhysicsObject in a usable state.
	 *
	 * @param collider A collider representing the shape and position of the
	 *                   object. Should be in allocated memory.
	 * @param velocity How fast this object is moving and in what direction.
	 */
	PhysicsObject(Collider* collider, const Vector3f& velocity) :
		m_position(collider->GetCenter()),
		m_oldPosition(collider->GetCenter()),
		m_velocity(velocity),
		m_collider(collider) {}

	PhysicsObject(const PhysicsObject& other);
	void operator=(PhysicsObject other);
	virtual ~PhysicsObject();

	/**
	 * Calculate this object's new location and properties after delta seconds
	 *
	 * @param delta How much time to simulate.
	 */
	void Integrate(float delta);

	/** Basic getter */
	inline const Vector3f& GetPosition() const { return m_position; }
	/** Basic getter */
	inline const Vector3f& GetVelocity() const { return m_velocity; }

	/**
	 * Returns a collider in the position of this object, updating the
	 * collider's position if necessary.
	 */
	inline const Collider& GetCollider()
	{
		//Find distance between current and old position
		Vector3f translation = m_position - m_oldPosition;
		//Update old position back to current position.
		m_oldPosition = m_position;
		//Move collider by distance moved.
		m_collider->Transform(translation);

		return *m_collider;
	}

	/** Basic setter */
	inline void SetVelocity(const Vector3f& velocity) { m_velocity = velocity; }

	/** Performs a Unit Test of this class */
	static void Test();
private:
	/** Where this object is in 3D space. */
	Vector3f m_position;
	/** The position of the object when the collider was last updated. */
	Vector3f m_oldPosition;
	/** How fast this object is moving and in what direction */
	Vector3f m_velocity;
	/** The collider representing the shape and position of this object. */
	Collider* m_collider;
};

#endif

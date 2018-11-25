#ifndef PLANE_INCLUDED_H
#define PLANE_INCLUDED_H

#include "../core/math3d.h"
#include "boundingSphere.h"

/**
 * The Plane class represents an infinitely large plane that can be used as
 * a collider in a physics engine.
 */
class Plane
{
public:
	/** 
	 * Creates a Plane in a usable state.
	 * 
	 * @param normal   The "up" direction from the plane's surface.
	 * @param distance The distance to the plane from the world origin 
	 *                   along the normal
	 */
	Plane(const Vector3f& normal, float distance) :
		m_normal(normal),
		m_distance(distance) {}

	/**
	 * Creates an equivalent plane with a normal at unit length and distance
	 * adjusted accordingly.
	 */
	Plane Normalized() const;

	/**
	 * Computes information about if this Plane intersects a Sphere.
	 *
	 * @param other The Sphere that's being tested for intersection with this
	 *                Plane.
	 */
	IntersectData IntersectSphere(const BoundingSphere& other) const;

	inline const Vector3f& GetNormal() const { return m_normal; }
	inline float GetDistance()         const { return m_distance; }

	/** Performs a Unit Test of this class */
	static void Test();
private:
	/** The "up" direction from the plane's surface. */
	const Vector3f m_normal;
	/** The distance to the plane from the world origin along the normal */
	const float    m_distance;
};

#endif


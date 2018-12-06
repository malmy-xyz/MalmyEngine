#ifndef AABB_INCLUDED_H
#define AABB_INCLUDED_H

#include "../maths/math3d.h"
#include "intersectData.h"

/**
 * The AABB class represents an Axis Aligned Bounding Box that can be used as
 * a collider in a physics engine.
 */
class AABB
{
public:
	/** 
	 * Creates an AABB in a usable state.
	 * 
	 * @param minExtents The corner of the AABB with the smallest coordinates.
	 * @param maxExtents The corner of the AABB with the largest coordinates.
	 */
	AABB(const Vector3f& minExtents, const Vector3f& maxExtents) :
		m_minExtents(minExtents),
		m_maxExtents(maxExtents) {}
	
	/**
	 * Computes information about if this AABB intersects another AABB.
	 *
	 * @param other The AABB that's being tested for intersection with this
	 *                AABB.
	 */
	IntersectData IntersectAABB(const AABB& other) const;

	/** Basic getter for the min extents */
	inline const Vector3f& GetMinExtents() const { return m_minExtents; }
	/** Basic getter for the max extents */
	inline const Vector3f& GetMaxExtents() const { return m_maxExtents; }

	/** Performs a Unit test of this class */
	static void Test();
private:
	/** The corner of the AABB with the smallest coordinates */
	const Vector3f m_minExtents;
	/** The corner of the AABB with the largest coordinates */
	const Vector3f m_maxExtents;
};

#endif

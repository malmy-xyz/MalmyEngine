#ifndef PHYSICS_ENGINE_INCLUDED_H
#define PHYSICS_ENGINE_INCLUDED_H

#include "physicsObject.h"
#include <vector>

/**
 * The PhysicsEngine encapsulates all the functions and information necessary
 * to perform a physics simulation.
 */
class PhysicsEngine
{
public:
	/** 
	 * Creates a PhysicsEngine in a usable state.
	 */
	PhysicsEngine() {}

	void AddObject(const PhysicsObject& object);
	
	/**
	 * Simulates the physics world for a certain period of time. Does not take
	 * object interaction into accounter; if two objects run into each other,
	 * they will pass through each other.
	 *
	 * If object interaction is desired, collision detection and response must
	 * be performed after this.
	 *
	 * @param delta How much time to simulate.
	 */
	void Simulate(float delta);

	/** 
	 * Finds all objects that have collided since the last step and updates
	 * them to adjust for the collision.
	 */
	void HandleCollisions();

	//TODO: Temporary Getters
	inline const PhysicsObject& GetObject(unsigned int index) const 
	{ 
		return m_objects[index]; 
	}
	inline unsigned int GetNumObjects() const 
	{ 
		return (unsigned int)m_objects.size();
	}
private:
	/** All the objects being simulated by the PhysicsEngine. */
	std::vector<PhysicsObject> m_objects;
};

#endif

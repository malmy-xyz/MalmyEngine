#ifndef PHYSICS_ENGINE_COMPONENT_INCLUDED_H
#define PHYSICS_ENGINE_COMPONENT_INCLUDED_H

#include "../core/entityComponent.h"
#include "../physics/physicsEngine.h"

/** rigidbody yap
Update i FixedUpdate Yap
*/

class PhysicsEngineComponent : public EntityComponent
{
public:
	PhysicsEngineComponent(const PhysicsEngine& engine) :
		m_physicsEngine(engine) {}

	virtual void Update(float delta);

	inline const PhysicsEngine& GetPhysicsEngine() { return m_physicsEngine; }
private:
	PhysicsEngine m_physicsEngine;
};


#endif

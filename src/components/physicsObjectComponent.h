#ifndef PHYSICS_OBJECT_COMPONENT_INCLUDED_H
#define PHYSICS_OBJECT_COMPONENT_INCLUDED_H

#include "../core/entityComponent.h"
#include "../physics/physicsEngine.h"

/** rigidbody yap
Update i FixedUpdate Yap
*/
class PhysicsObjectComponent : public EntityComponent
{
public:
	PhysicsObjectComponent(const PhysicsObject* object) :
		m_physicsObject(object) {}

	virtual void Update(float delta);
private:
	const PhysicsObject* m_physicsObject;
};


#endif

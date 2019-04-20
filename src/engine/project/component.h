#pragma once


#include "engine/malmy.h"


namespace Malmy
{


struct IScene;


struct MALMY_ENGINE_API ComponentUID final
{

	ComponentUID()
	{
		scene = nullptr;
		entity = INVALID_ENTITY;
		type = {-1};
	}

	ComponentUID(Entity _entity, ComponentType _type, IScene* _scene)
		: entity(_entity)
		, type(_type)
		, scene(_scene)
	{
	}

	Entity entity; 
	ComponentType type;
	IScene* scene;

	static const ComponentUID INVALID;

	bool operator==(const ComponentUID& rhs) const
	{
		return type == rhs.type && scene == rhs.scene && entity == rhs.entity;
	}
	bool isValid() const { return entity.isValid(); }
};


} // namespace Malmy

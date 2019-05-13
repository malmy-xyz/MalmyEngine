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
		gameobject = INVALID_GAMEOBJECT;
		type = {-1};
	}

	ComponentUID(GameObject _gameobject, ComponentType _type, IScene* _scene)
		: gameobject(_gameobject)
		, type(_type)
		, scene(_scene)
	{
	}

	GameObject gameobject; 
	ComponentType type;
	IScene* scene;

	static const ComponentUID INVALID;

	bool operator==(const ComponentUID& rhs) const
	{
		return type == rhs.type && scene == rhs.scene && gameobject == rhs.gameobject;
	}
	bool isValid() const { return gameobject.isValid(); }
};


} // namespace Malmy

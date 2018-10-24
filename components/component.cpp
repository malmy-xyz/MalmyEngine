#include "component.h"


Component::Component()
{
	//
}

Component::~Component()
{
	//
}


void Component::Start()
{
	if (m_Enable)
	{
		//
		Debug::Log("Component::Start ");
	}
	//enable ise
}

void Component::Update()
{
	//her framede cagrilcak
	Debug::Log("Component::Update ");

}
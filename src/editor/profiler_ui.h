#pragma once


#include "engine/malmy.h"


namespace Malmy
{

	
class Engine;


class ProfilerUI
{
public:
	virtual ~ProfilerUI() {}
	virtual void onGUI() = 0;

	static ProfilerUI* create(Engine& engine);
	static void destroy(ProfilerUI& ui);

	bool m_is_open;
};


} // namespace Malmy

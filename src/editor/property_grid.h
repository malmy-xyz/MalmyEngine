#pragma once


#include "engine/array.h"


namespace Malmy
{


class ArrayDescriptorBase;
struct ComponentUID;
struct IEnumPropertyDescriptor;
class PropertyDescriptorBase;
struct ISampledFunctionDescriptor;
class WorldEditor;
class StudioApp;


class MALMY_EDITOR_API PropertyGrid
{
friend struct GridUIVisitor;
public:
	struct IPlugin
	{
		virtual ~IPlugin() {}
		virtual void onGUI(PropertyGrid& grid, ComponentUID cmp) = 0;
	};

public:
	explicit PropertyGrid(StudioApp& app);
	~PropertyGrid();

	void addPlugin(IPlugin& plugin) { m_plugins.push(&plugin); }
	void removePlugin(IPlugin& plugin) { m_plugins.eraseItem(&plugin); }
	void onGUI();
	bool gameobjectInput(const char* label, const char* str_id, GameObject& gameobject);

public:
	bool m_is_open;

private:
	void showComponentProperties(const Array<GameObject>& entities, ComponentType cmp_type);
	void showCoreProperties(const Array<GameObject>& entities) const;

private:
	StudioApp& m_app;
	WorldEditor& m_editor;
	Array<IPlugin*> m_plugins;
	GameObject m_deferred_select;
	
	char m_component_filter[32];

	float m_particle_emitter_timescale;
	bool m_particle_emitter_updating;
};


} // namespace Malmy
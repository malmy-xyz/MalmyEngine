#pragma once
#include "engine/malmy.h"

namespace Malmy
{

	class Engine;
	class InputBlob;
	struct IPlugin;
	class JsonSerializer;
	class OutputBlob;
	class Project;
	template <typename T> class Array;
	template <typename T> class DelegateList;

	class MALMY_ENGINE_API PluginManager
	{
	public:
		virtual ~PluginManager() {}

		static PluginManager* create(Engine& engine);
		static void destroy(PluginManager* manager);

		virtual void unload(IPlugin* plugin) = 0;
		virtual IPlugin* load(const char* path) = 0;
		virtual void addPlugin(IPlugin* plugin) = 0;
		virtual void update(float dt, bool paused) = 0;
		virtual void serialize(OutputBlob& serializer) = 0;
		virtual void deserialize(InputBlob& serializer) = 0;
		virtual IPlugin* getPlugin(const char* name) = 0;
		virtual const Array<IPlugin*>& getPlugins() const = 0;
		virtual const Array<void*>& getLibraries() const = 0;
		virtual void* getLibrary(IPlugin* plugin) const = 0;
		virtual DelegateList<void(void*)>& libraryLoaded() = 0;
	};

} // namespace Malmy

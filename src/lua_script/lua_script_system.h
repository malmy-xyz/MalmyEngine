#pragma once
#include "engine/iplugin.h"
#include "engine/path.h"
#include "engine/resource.h"
#include "engine/string.h"

struct lua_State;

namespace Malmy
{

	class LuaScript;

	class LuaScriptScene : public IScene
	{
	public:
		struct Property
		{
			enum Type : int
			{
				BOOLEAN,
				FLOAT,
				GAMEOBJECT,
				RESOURCE,
				STRING,
				ANY
			};

			explicit Property(IAllocator& allocator)
				: stored_value(allocator)
			{
			}

			u32 name_hash;
			Type type;
			ResourceType resource_type;
			string stored_value;
		};

		struct IFunctionCall
		{
			virtual ~IFunctionCall() {}
			virtual void add(int parameter) = 0;
			virtual void add(bool parameter) = 0;
			virtual void add(float parameter) = 0;
			virtual void add(void* parameter) = 0;
			virtual void addEnvironment(int env) = 0;
		};

		typedef int(*lua_CFunction) (lua_State *L);

	public:
		virtual Path getScriptPath(GameObject gameobject, int scr_index) = 0;
		virtual void setScriptPath(GameObject gameobject, int scr_index, const Path& path) = 0;
		virtual int getEnvironment(GameObject gameobject, int scr_index) = 0;
		virtual IFunctionCall* beginFunctionCall(GameObject gameobject, int scr_index, const char* function) = 0;
		virtual void endFunctionCall() = 0;
		virtual int getScriptCount(GameObject gameobject) = 0;
		virtual lua_State* getState(GameObject gameobject, int scr_index) = 0;
		virtual void insertScript(GameObject gameobject, int idx) = 0;
		virtual int addScript(GameObject gameobject) = 0;
		virtual void removeScript(GameObject gameobject, int scr_index) = 0;
		virtual void enableScript(GameObject gameobject, int scr_index, bool enable) = 0;
		virtual bool isScriptEnabled(GameObject gameobject, int scr_index) const = 0;
		virtual void moveScript(GameObject gameobject, int scr_index, bool up) = 0;
		virtual void serializeScript(GameObject gameobject, int scr_index, OutputBlob& blob) = 0;
		virtual void deserializeScript(GameObject gameobject, int scr_index, InputBlob& blob) = 0;
		virtual void setPropertyValue(GameObject gameobject, int scr_index, const char* name, const char* value) = 0;
		virtual void getPropertyValue(GameObject gameobject, int scr_index, const char* property_name, char* out, int max_size) = 0;
		virtual int getPropertyCount(GameObject gameobject, int scr_index) = 0;
		virtual const char* getPropertyName(GameObject gameobject, int scr_index, int prop_index) = 0;
		virtual Property::Type getPropertyType(GameObject gameobject, int scr_index, int prop_index) = 0;
		virtual ResourceType getPropertyResourceType(GameObject gameobject, int scr_index, int prop_index) = 0;
		virtual void getScriptData(GameObject gameobject, OutputBlob& blob) = 0;
		virtual void setScriptData(GameObject gameobject, InputBlob& blob) = 0;
	};

} // namespace Malmy

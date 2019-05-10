#pragma once


#include "engine/resource.h"
#include "engine/resource_manager_base.h"
#include "engine/string.h"


namespace Malmy
{


	class LuaScript MALMY_FINAL : public Resource
	{
	public:
		LuaScript(const Path& path, ResourceManagerBase& resource_manager, IAllocator& allocator);
		virtual ~LuaScript();

		ResourceType getType() const override { return TYPE; }

		void unload() override;
		bool load(FS::IFile& file) override;
		const char* getSourceCode() const { return m_source_code.c_str(); }

		static const ResourceType TYPE;

	private:
		string m_source_code;
	};


	class LuaScriptManager MALMY_FINAL : public ResourceManagerBase
	{
	public:
		explicit LuaScriptManager(IAllocator& allocator);
		~LuaScriptManager();

	protected:
		Resource* createResource(const Path& path) override;
		void destroyResource(Resource& resource) override;

	private:
		IAllocator& m_allocator;
	};


} // namespace Malmy

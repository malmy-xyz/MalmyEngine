#include "lua_script_manager.h"
#include "engine/log.h"
#include "engine/fs/file_system.h"

//
namespace Malmy
{

	const ResourceType LuaScript::TYPE("lua_script");

	//olusurema
	//path, resource manager ref, allacator aliyor
	//ataam yapiliyor bunlar yerlerine
	LuaScript::LuaScript(const Path& path, ResourceManagerBase& resource_manager, IAllocator& allocator)
		: Resource(path, resource_manager, allocator)
		, m_source_code(allocator)
	{
	}

	LuaScript::~LuaScript() = default;

	//silme islmei asuyilir
	void LuaScript::unload()
	{
		m_source_code = "";
	}

	//dosya yolunda lua code unu yukluyoz
	//source code ye atiyor
	bool LuaScript::load(FS::IFile& file)
	{
		m_source_code.set((const char*)file.getBuffer(), (int)file.size());
		m_size = file.size();
		return true;
	}

	//script manager con
	//
	LuaScriptManager::LuaScriptManager(IAllocator& allocator)
		: ResourceManagerBase(allocator)
		, m_allocator(allocator)
	{
	}

	//script manager dec
	LuaScriptManager::~LuaScriptManager() = default;

	//yeni lua script olustur
	Resource* LuaScriptManager::createResource(const Path& path)
	{
		return MALMY_NEW(m_allocator, LuaScript)(path, *this, m_allocator);
	}

	//lua scripti sildik
	void LuaScriptManager::destroyResource(Resource& resource)
	{
		MALMY_DELETE(m_allocator, static_cast<LuaScript*>(&resource));
	}

} // namespace Malmy
#include "engine/malmy.h"
#include "renderer/shader_manager.h"

#include "engine/resource.h"
#include "renderer/shader.h"

namespace Malmy
{


ShaderManager::ShaderManager(Renderer& renderer, IAllocator& allocator)
	: ResourceManagerBase(allocator)
	, m_allocator(allocator)
	, m_renderer(renderer)
{
	m_buffer = nullptr;
	m_buffer_size = -1;
}


ShaderManager::~ShaderManager()
{
	MALMY_DELETE(m_allocator, m_buffer);
}


Resource* ShaderManager::createResource(const Path& path)
{
	return MALMY_NEW(m_allocator, Shader)(path, *this, m_allocator);
}


void ShaderManager::destroyResource(Resource& resource)
{
	MALMY_DELETE(m_allocator, static_cast<Shader*>(&resource));
}


u8* ShaderManager::getBuffer(i32 size)
{
	if (m_buffer_size < size)
	{
		MALMY_DELETE(m_allocator, m_buffer);
		m_buffer = nullptr;
		m_buffer_size = -1;
	}
	if (m_buffer == nullptr)
	{
		m_buffer = (u8*)m_allocator.allocate(sizeof(u8) * size);
		m_buffer_size = size;
	}
	return m_buffer;
}


ShaderBinaryManager::ShaderBinaryManager(Renderer& renderer, IAllocator& allocator)
	: ResourceManagerBase(allocator)
	, m_allocator(allocator)
{
}


ShaderBinaryManager::~ShaderBinaryManager() = default;


Resource* ShaderBinaryManager::createResource(const Path& path)
{
	return MALMY_NEW(m_allocator, ShaderBinary)(path, *this, m_allocator);
}


void ShaderBinaryManager::destroyResource(Resource& resource)
{
	MALMY_DELETE(m_allocator, static_cast<ShaderBinary*>(&resource));
}


} // namespace Malmy

#include "engine/malmy.h"
#include "renderer/material_manager.h"

#include "engine/resource.h"
#include "renderer/material.h"

namespace Malmy
{
	Resource* MaterialManager::createResource(const Path& path)
	{
		return MALMY_NEW(m_allocator, Material)(path, *this, m_allocator);
	}

	void MaterialManager::destroyResource(Resource& resource)
	{
		MALMY_DELETE(m_allocator, static_cast<Material*>(&resource));
	}
}
#include "engine/malmy.h"
#include "renderer/model_manager.h"

#include "engine/resource.h"
#include "renderer/model.h"

namespace Malmy
{
	Resource* ModelManager::createResource(const Path& path)
	{
		return MALMY_NEW(m_allocator, Model)(path, *this, m_renderer, m_allocator);
	}

	void ModelManager::destroyResource(Resource& resource)
	{
		MALMY_DELETE(m_allocator, static_cast<Model*>(&resource));
	}
}
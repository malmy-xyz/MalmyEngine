#pragma once

#include "engine/resource_manager_base.h"

namespace Malmy
{

	class Renderer;


	class MALMY_RENDERER_API ModelManager MALMY_FINAL : public ResourceManagerBase
	{
	public:
		ModelManager(Renderer& renderer, IAllocator& allocator)
			: ResourceManagerBase(allocator)
			, m_allocator(allocator)
			, m_renderer(renderer)
		{}

		~ModelManager() {}

	protected:
		Resource* createResource(const Path& path) override;
		void destroyResource(Resource& resource) override;

	private:
		IAllocator& m_allocator;
		Renderer& m_renderer;
	};
}
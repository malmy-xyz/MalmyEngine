#pragma once

#include "engine/resource_manager_base.h"

namespace Malmy
{

	class Renderer;

	class MALMY_RENDERER_API MaterialManager MALMY_FINAL : public ResourceManagerBase
	{
	public:
		MaterialManager(Renderer& renderer, IAllocator& allocator)
			: ResourceManagerBase(allocator)
			, m_renderer(renderer)
			, m_allocator(allocator)
		{}
		~MaterialManager() {}

		Renderer& getRenderer() { return m_renderer; }

	protected:
		Resource* createResource(const Path& path) override;
		void destroyResource(Resource& resource) override;

	private:
		IAllocator& m_allocator;
		Renderer& m_renderer;
	};
}
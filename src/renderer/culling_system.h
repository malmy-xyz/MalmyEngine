#pragma once


#include "engine/malmy.h"


namespace Malmy
{
	template <typename T> class Array;
	struct IAllocator;
	struct Sphere;
	struct Vec3;


	struct Frustum;

	class MALMY_RENDERER_API CullingSystem
	{
	public:
		typedef Array<Sphere> InputSpheres;
		typedef Array<GameObject> Subresults;
		typedef Array<Subresults> Results;

		CullingSystem() { }
		virtual ~CullingSystem() { }

		static CullingSystem* create(IAllocator& allocator);
		static void destroy(CullingSystem& culling_system);

		virtual void clear() = 0;
		virtual const Results& getResult() = 0;

		virtual Results& cull(const Frustum& frustum, u64 layer_mask) = 0;

		virtual bool isAdded(GameObject model_instance) = 0;
		virtual void addStatic(GameObject model_instance, const Sphere& sphere, u64 layer_mask) = 0;
		virtual void removeStatic(GameObject model_instance) = 0;

		virtual void setLayerMask(GameObject model_instance, u64 layer) = 0;
		virtual u64 getLayerMask(GameObject model_instance) = 0;

		virtual void updateBoundingSphere(const Sphere& sphere, GameObject model_instance) = 0;

		virtual void insert(const InputSpheres& spheres, const Array<GameObject>& model_instances) = 0;
		virtual const Sphere& getSphere(GameObject model_instance) = 0;
	};
} // namespace Lux

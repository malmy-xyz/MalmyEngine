#include "physics/physics_system.h"

#include <PxPhysicsAPI.h>

#include "cooking/PxCooking.h"
#include "engine/base_proxy_allocator.h"
#include "engine/log.h"
#include "engine/engine.h"
#include "engine/reflection.h"
#include "engine/project/project.h"
#include "physics/physics_geometry_manager.h"
#include "physics/physics_scene.h"
#include "renderer/texture.h"
#include <cstdlib>


namespace Malmy
{
	static void registerProperties(IAllocator& allocator)
	{
		using namespace Reflection;

		static auto dynamicTypeDesc = enumDesciptor<PhysicsScene::DynamicType>(
			MALMY_ENUM_VALUE(PhysicsScene::DynamicType::DYNAMIC),
			MALMY_ENUM_VALUE(PhysicsScene::DynamicType::STATIC),
			MALMY_ENUM_VALUE(PhysicsScene::DynamicType::KINEMATIC)
		);
		registerEnum(dynamicTypeDesc);

		static auto d6MotionNameDesc = enumDesciptor<PhysicsScene::D6Motion>(
			MALMY_ENUM_VALUE(PhysicsScene::D6Motion::LOCKED),
			MALMY_ENUM_VALUE(PhysicsScene::D6Motion::LIMITED),
			MALMY_ENUM_VALUE(PhysicsScene::D6Motion::FREE)
		);
		registerEnum(d6MotionNameDesc);

		static auto phy_scene = scene("physics",
			functions(
				function(MALMY_FUNC(PhysicsScene::raycast)),
				function(MALMY_FUNC(PhysicsScene::raycastEx))
			),
			component("ragdoll",
				blob_property("data", MALMY_PROP(PhysicsScene, RagdollData)),
				property("Layer", MALMY_PROP(PhysicsScene, RagdollLayer))
			),
			component("sphere_rigid_actor",
				functions(
					function(MALMY_FUNC(PhysicsScene::applyForceToActor)),
					function(MALMY_FUNC(PhysicsScene::applyImpulseToActor)),
					function(MALMY_FUNC(PhysicsScene::getActorVelocity))
				),
				property("Radius", MALMY_PROP(PhysicsScene, SphereRadius),
					MinAttribute(0)),
				property("Layer", MALMY_PROP(PhysicsScene, ActorLayer)),
				enum_property("Dynamic", MALMY_PROP(PhysicsScene, DynamicType), dynamicTypeDesc),
				property("Trigger", MALMY_PROP(PhysicsScene, IsTrigger)) 
			),
			component("capsule_rigid_actor",
				functions(
					function(MALMY_FUNC(PhysicsScene::applyForceToActor)),
					function(MALMY_FUNC(PhysicsScene::applyImpulseToActor)),
					function(MALMY_FUNC(PhysicsScene::getActorVelocity))
				),
				property("Radius", MALMY_PROP(PhysicsScene, CapsuleRadius),
					MinAttribute(0)),
				property("Height", MALMY_PROP(PhysicsScene, CapsuleHeight)),
				enum_property("Dynamic", MALMY_PROP(PhysicsScene, DynamicType), dynamicTypeDesc),
				property("Trigger", MALMY_PROP(PhysicsScene, IsTrigger))
			),
			component("d6_joint",
				property("Connected body", MALMY_PROP(PhysicsScene, JointConnectedBody)),
				property("Axis position", MALMY_PROP(PhysicsScene, JointAxisPosition)),
				property("Axis direction", MALMY_PROP(PhysicsScene, JointAxisDirection)),
				enum_property("X motion", MALMY_PROP(PhysicsScene, D6JointXMotion), d6MotionNameDesc),
				enum_property("Y motion", MALMY_PROP(PhysicsScene, D6JointYMotion), d6MotionNameDesc),
				enum_property("Z motion", MALMY_PROP(PhysicsScene, D6JointZMotion), d6MotionNameDesc),
				enum_property("Swing 1", MALMY_PROP(PhysicsScene, D6JointSwing1Motion), d6MotionNameDesc),
				enum_property("Swing 2", MALMY_PROP(PhysicsScene, D6JointSwing2Motion), d6MotionNameDesc),
				enum_property("Twist", MALMY_PROP(PhysicsScene, D6JointTwistMotion), d6MotionNameDesc),
				property("Linear limit", MALMY_PROP(PhysicsScene, D6JointLinearLimit),
					MinAttribute(0)),
				property("Swing limit", MALMY_PROP(PhysicsScene, D6JointSwingLimit),
					RadiansAttribute()),
				property("Twist limit", MALMY_PROP(PhysicsScene, D6JointTwistLimit),
					RadiansAttribute()),
				property("Damping", MALMY_PROP(PhysicsScene, D6JointDamping)),
				property("Stiffness", MALMY_PROP(PhysicsScene, D6JointStiffness)),
				property("Restitution", MALMY_PROP(PhysicsScene, D6JointRestitution))
			),
			component("spherical_joint",
				property("Connected body", MALMY_PROP(PhysicsScene, JointConnectedBody)),
				property("Axis position", MALMY_PROP(PhysicsScene, JointAxisPosition)),
				property("Axis direction", MALMY_PROP(PhysicsScene, JointAxisDirection)),
				property("Use limit", MALMY_PROP(PhysicsScene, SphericalJointUseLimit)),
				property("Limit", MALMY_PROP(PhysicsScene, SphericalJointLimit),
					RadiansAttribute())
			),
			component("distance_joint",
				property("Connected body", MALMY_PROP(PhysicsScene, JointConnectedBody)),
				property("Axis position", MALMY_PROP(PhysicsScene, JointAxisPosition)),
				property("Damping", MALMY_PROP(PhysicsScene, DistanceJointDamping),
					MinAttribute(0)),
				property("Stiffness", MALMY_PROP(PhysicsScene, DistanceJointStiffness),
					MinAttribute(0)),
				property("Tolerance", MALMY_PROP(PhysicsScene, DistanceJointTolerance),
					MinAttribute(0)),
				property("Limits", MALMY_PROP(PhysicsScene, DistanceJointLimits))
			),
			component("hinge_joint",
				property("Connected body", MALMY_PROP(PhysicsScene, JointConnectedBody)),
				property("Damping", MALMY_PROP(PhysicsScene, HingeJointDamping),
					MinAttribute(0)),
				property("Stiffness", MALMY_PROP(PhysicsScene, HingeJointStiffness),
					MinAttribute(0)),
				property("Axis position", MALMY_PROP(PhysicsScene, JointAxisPosition)), 
				property("Axis direction", MALMY_PROP(PhysicsScene, JointAxisDirection)),
				property("Use limit", MALMY_PROP(PhysicsScene, HingeJointUseLimit)),
				property("Limit", MALMY_PROP(PhysicsScene, HingeJointLimit),
					RadiansAttribute())
			),
			component("physical_controller",
				functions(
					function(MALMY_FUNC(PhysicsScene::moveController))
				),
				property("Radius", MALMY_PROP(PhysicsScene, ControllerRadius)),
				property("Height", MALMY_PROP(PhysicsScene, ControllerHeight)),
				property("Layer", MALMY_PROP(PhysicsScene, ControllerLayer)),
				property("Use Custom Gravity", MALMY_PROP(PhysicsScene, ControllerCustomGravity)),
				property("Custom Gravity Acceleration", MALMY_PROP(PhysicsScene, ControllerCustomGravityAcceleration))
			),
			component("rigid_actor",
				property("Layer", MALMY_PROP(PhysicsScene, ActorLayer)),
				enum_property("Dynamic", MALMY_PROP(PhysicsScene, DynamicType), dynamicTypeDesc),
				property("Trigger", MALMY_PROP(PhysicsScene, IsTrigger)),
				array("Box geometry", &PhysicsScene::getBoxGeometryCount, &PhysicsScene::addBoxGeometry, &PhysicsScene::removeBoxGeometry,
					property("Size", MALMY_PROP(PhysicsScene, BoxGeomHalfExtents)),
					property("Position offset", MALMY_PROP(PhysicsScene, BoxGeomOffsetPosition)),
					property("Rotation offset", MALMY_PROP(PhysicsScene, BoxGeomOffsetRotation),
						RadiansAttribute())
				),
				array("Sphere geometry", &PhysicsScene::getBoxGeometryCount, &PhysicsScene::addBoxGeometry, &PhysicsScene::removeBoxGeometry,
					property("Radius", MALMY_PROP(PhysicsScene, SphereGeomRadius),
						MinAttribute(0)),
					property("Position offset", MALMY_PROP(PhysicsScene, SphereGeomOffsetPosition)),
					property("Rotation offset", MALMY_PROP(PhysicsScene, SphereGeomOffsetRotation),
						RadiansAttribute())
				)
			),
			component("box_rigid_actor",
				functions(
					function(MALMY_FUNC(PhysicsScene::applyForceToActor)),
					function(MALMY_FUNC(PhysicsScene::applyImpulseToActor)),
					function(MALMY_FUNC(PhysicsScene::getActorVelocity))
				),
				property("Layer", MALMY_PROP(PhysicsScene, ActorLayer)),
				enum_property("Dynamic", MALMY_PROP(PhysicsScene, DynamicType), dynamicTypeDesc),
				property("Trigger", MALMY_PROP(PhysicsScene, IsTrigger)),
				property("Size", MALMY_PROP(PhysicsScene, HalfExtents))
			),
			component("mesh_rigid_actor",
				functions(
					function(MALMY_FUNC(PhysicsScene::applyForceToActor)),
					function(MALMY_FUNC(PhysicsScene::applyImpulseToActor)),
					function(MALMY_FUNC(PhysicsScene::getActorVelocity))
				),
				property("Layer", MALMY_PROP(PhysicsScene, ActorLayer)),
				enum_property("Dynamic", MALMY_PROP(PhysicsScene, DynamicType), dynamicTypeDesc),
				property("Source", MALMY_PROP(PhysicsScene, ShapeSource),
					ResourceAttribute("Physics (*.phy)", PhysicsGeometry::TYPE))
			),
			component("physical_heightfield",
				property("Layer", MALMY_PROP(PhysicsScene, HeightfieldLayer)),
				property("Heightmap", MALMY_PROP(PhysicsScene, HeightmapSource),
					ResourceAttribute("Image (*.raw)", Texture::TYPE)),
				property("Y scale", MALMY_PROP(PhysicsScene, HeightmapYScale),
					MinAttribute(0)),
				property("XZ scale", MALMY_PROP(PhysicsScene, HeightmapXZScale),
					MinAttribute(0))
			)
		);
		registerScene(phy_scene);
	}


	struct CustomErrorCallback : public physx::PxErrorCallback
	{
		void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line) override
		{
			g_log_error.log("Physics") << message;
		}
	};


	class AssertNullAllocator : public physx::PxAllocatorCallback
	{
	public:
		#ifdef _WIN32
			void* allocate(size_t size, const char*, const char*, int) override
			{
				void* ret = _aligned_malloc(size, 16);
				// g_log_info.log("Physics") << "Allocated " << size << " bytes for " << typeName << "
				// from " << filename << "(" << line << ")";
				ASSERT(ret);
				return ret;
			}
			void deallocate(void* ptr) override { _aligned_free(ptr); }
		#else
			void* allocate(size_t size, const char*, const char*, int) override
			{
				void* ret = aligned_alloc(16, size);
				// g_log_info.log("Physics") << "Allocated " << size << " bytes for " << typeName << "
				// from " << filename << "(" << line << ")";
				ASSERT(ret);
				return ret;
			}
			void deallocate(void* ptr) override { free(ptr); }
		#endif
	};


	struct PhysicsSystemImpl MALMY_FINAL : public PhysicsSystem
	{
		explicit PhysicsSystemImpl(Engine& engine)
			: m_allocator(engine.getAllocator())
			, m_engine(engine)
			, m_manager(*this, engine.getAllocator())
		{
			registerProperties(engine.getAllocator());
			m_manager.create(PhysicsGeometry::TYPE, engine.getResourceManager());
			PhysicsScene::registerLuaAPI(m_engine.getState());

			m_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_physx_allocator, m_error_callback);

			m_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_foundation, physx::PxTolerancesScale());

			physx::PxTolerancesScale scale;
			m_cooking = PxCreateCooking(PX_PHYSICS_VERSION, *m_foundation, physx::PxCookingParams(scale));
			connect2VisualDebugger();
		}


		~PhysicsSystemImpl()
		{
			m_cooking->release();
			m_physics->release();
			m_foundation->release();
		}


		void createScenes(Project& project) override
		{
			auto* scene = PhysicsScene::create(*this, project, m_engine, m_allocator);
			project.addScene(scene);
		}


		void destroyScene(IScene* scene) override { PhysicsScene::destroy(static_cast<PhysicsScene*>(scene)); }


		physx::PxPhysics* getPhysics() override
		{
			return m_physics;
		}

		physx::PxCooking* getCooking() override
		{
			return m_cooking;
		}

		bool connect2VisualDebugger()
		{
			/*if (m_physics->getPvdConnectionManager() == nullptr) return false;

			const char* pvd_host_ip = "127.0.0.1";
			int port = 5425;
			unsigned int timeout = 100;
			physx::PxVisualDebuggerConnectionFlags connectionFlags =
				physx::PxVisualDebuggerExt::getAllConnectionFlags();

			auto* connection = physx::PxVisualDebuggerExt::createConnection(
				m_physics->getPvdConnectionManager(), pvd_host_ip, port, timeout, connectionFlags);
			return connection != nullptr;*/
			return false;
		}

		physx::PxPhysics* m_physics;
		physx::PxFoundation* m_foundation;
		physx::PxControllerManager* m_controller_manager;
		AssertNullAllocator m_physx_allocator;
		CustomErrorCallback m_error_callback;
		physx::PxCooking* m_cooking;
		PhysicsGeometryManager m_manager;
		Engine& m_engine;
		BaseProxyAllocator m_allocator;
	};


	MALMY_PLUGIN_ENTRY(physics)
	{
		return MALMY_NEW(engine.getAllocator(), PhysicsSystemImpl)(engine);
	}


} // namespace Malmy




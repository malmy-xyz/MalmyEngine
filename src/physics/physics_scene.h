#pragma once


#include "engine/malmy.h"
#include "engine/iplugin.h"
#include "engine/vec.h"


#ifdef STATIC_PLUGINS
	#define MALMY_PHYSICS_API
#elif defined BUILDING_PHYSICS
	#define MALMY_PHYSICS_API MALMY_LIBRARY_EXPORT
#else
	#define MALMY_PHYSICS_API MALMY_LIBRARY_IMPORT
#endif


struct lua_State;


namespace physx
{
	class PxJoint;
}


namespace Malmy
{


class Engine;
struct IAllocator;
struct Matrix;
class Path;
class PhysicsSystem;
struct Quat;
struct RagdollBone;
struct RigidTransform;
class Project;
template <typename T> class DelegateList;


struct RaycastHit
{
	Vec3 position;
	Vec3 normal;
	GameObject gameobject;
};


class MALMY_PHYSICS_API PhysicsScene : public IScene
{
public:
	enum class D6Motion : int
	{
		LOCKED,
		LIMITED,
		FREE
	};
	enum class ActorType
	{
		BOX,
		MESH,
		CAPSULE,
		SPHERE,
		RIGID
	};
	enum class BoneOrientation : int
	{
		X,
		Y
	};
	enum class DynamicType : int
	{
		STATIC,
		DYNAMIC,
		KINEMATIC
	};
	
	struct ContactData
	{
		Vec3 position;
		GameObject e1;
		GameObject e2;
	};

	typedef int ContactCallbackHandle;

	static PhysicsScene* create(PhysicsSystem& system, Project& context, Engine& engine, IAllocator& allocator);
	static void destroy(PhysicsScene* scene);
	static void registerLuaAPI(lua_State* L);

	virtual ~PhysicsScene() {}
	virtual void render() = 0;
	virtual GameObject raycast(const Vec3& origin, const Vec3& dir, GameObject ignore_gameobject) = 0;
	virtual bool raycastEx(const Vec3& origin, const Vec3& dir, float distance, RaycastHit& result, GameObject ignored, int layer) = 0;
	virtual PhysicsSystem& getSystem() const = 0;

	virtual DelegateList<void(const ContactData&)>& onCollision() = 0;
	virtual void setActorLayer(GameObject gameobject, int layer) = 0;
	virtual int getActorLayer(GameObject gameobject) = 0;
	virtual bool getIsTrigger(GameObject gameobject) = 0;
	virtual void setIsTrigger(GameObject gameobject, bool is_trigger) = 0;
	virtual DynamicType getDynamicType(GameObject gameobject) = 0;
	virtual void setDynamicType(GameObject gameobject, DynamicType) = 0;
	virtual Vec3 getHalfExtents(GameObject gameobject) = 0;
	virtual void setHalfExtents(GameObject gameobject, const Vec3& size) = 0;
	virtual Path getShapeSource(GameObject gameobject) = 0;
	virtual void setShapeSource(GameObject gameobject, const Path& str) = 0;
	virtual Path getHeightmapSource(GameObject gameobject) = 0;
	virtual void setHeightmapSource(GameObject gameobject, const Path& path) = 0;
	virtual float getHeightmapXZScale(GameObject gameobject) = 0;
	virtual void setHeightmapXZScale(GameObject gameobject, float scale) = 0;
	virtual float getHeightmapYScale(GameObject gameobject) = 0;
	virtual void setHeightmapYScale(GameObject gameobject, float scale) = 0;
	virtual int getHeightfieldLayer(GameObject gameobject) = 0;
	virtual void setHeightfieldLayer(GameObject gameobject, int layer) = 0;
	virtual void updateHeighfieldData(GameObject gameobject,
		int x,
		int y,
		int w,
		int h,
		const u8* data,
		int bytes_per_pixel) = 0;

	virtual float getCapsuleRadius(GameObject gameobject) = 0;
	virtual void setCapsuleRadius(GameObject gameobject, float value) = 0;
	virtual float getCapsuleHeight(GameObject gameobject) = 0;
	virtual void setCapsuleHeight(GameObject gameobject, float value) = 0;

	virtual float getSphereRadius(GameObject gameobject) = 0;
	virtual void setSphereRadius(GameObject gameobject, float value) = 0;

	virtual D6Motion getD6JointXMotion(GameObject gameobject) = 0;
	virtual void setD6JointXMotion(GameObject gameobject, D6Motion motion) = 0;
	virtual D6Motion getD6JointYMotion(GameObject gameobject) = 0;
	virtual void setD6JointYMotion(GameObject gameobject, D6Motion motion) = 0;
	virtual D6Motion getD6JointZMotion(GameObject gameobject) = 0;
	virtual void setD6JointZMotion(GameObject gameobject, D6Motion motion) = 0;
	virtual D6Motion getD6JointSwing1Motion(GameObject gameobject) = 0;
	virtual void setD6JointSwing1Motion(GameObject gameobject, D6Motion motion) = 0;
	virtual D6Motion getD6JointSwing2Motion(GameObject gameobject) = 0;
	virtual void setD6JointSwing2Motion(GameObject gameobject, D6Motion motion) = 0;
	virtual D6Motion getD6JointTwistMotion(GameObject gameobject) = 0;
	virtual void setD6JointTwistMotion(GameObject gameobject, D6Motion motion) = 0;
	virtual float getD6JointLinearLimit(GameObject gameobject) = 0;
	virtual void setD6JointLinearLimit(GameObject gameobject, float limit) = 0;
	virtual Vec2 getD6JointTwistLimit(GameObject gameobject) = 0;
	virtual void setD6JointTwistLimit(GameObject gameobject, const Vec2& limit) = 0;
	virtual Vec2 getD6JointSwingLimit(GameObject gameobject) = 0;
	virtual void setD6JointSwingLimit(GameObject gameobject, const Vec2& limit) = 0;
	virtual float getD6JointDamping(GameObject gameobject) = 0;
	virtual void setD6JointDamping(GameObject gameobject, float value) = 0;
	virtual float getD6JointStiffness(GameObject gameobject) = 0;
	virtual void setD6JointStiffness(GameObject gameobject, float value) = 0;
	virtual float getD6JointRestitution(GameObject gameobject) = 0;
	virtual void setD6JointRestitution(GameObject gameobject, float value) = 0;

	virtual float getDistanceJointDamping(GameObject gameobject) = 0;
	virtual void setDistanceJointDamping(GameObject gameobject, float value) = 0;
	virtual float getDistanceJointStiffness(GameObject gameobject) = 0;
	virtual void setDistanceJointStiffness(GameObject gameobject, float value) = 0;
	virtual float getDistanceJointTolerance(GameObject gameobject) = 0;
	virtual void setDistanceJointTolerance(GameObject gameobject, float value) = 0;
	virtual Vec2 getDistanceJointLimits(GameObject gameobject) = 0;
	virtual void setDistanceJointLimits(GameObject gameobject, const Vec2& value) = 0;
	virtual Vec3 getDistanceJointLinearForce(GameObject gameobject) = 0;
	virtual int getJointCount() = 0;
	virtual GameObject getJointGameObject(int index) = 0;

	virtual float getHingeJointDamping(GameObject gameobject) = 0;
	virtual void setHingeJointDamping(GameObject gameobject, float value) = 0;
	virtual float getHingeJointStiffness(GameObject gameobject) = 0;
	virtual void setHingeJointStiffness(GameObject gameobject, float value) = 0;
	virtual bool getHingeJointUseLimit(GameObject gameobject) = 0;
	virtual void setHingeJointUseLimit(GameObject gameobject, bool use_limit) = 0;
	virtual Vec2 getHingeJointLimit(GameObject gameobject) = 0;
	virtual void setHingeJointLimit(GameObject gameobject, const Vec2& limit) = 0;

	virtual GameObject getJointConnectedBody(GameObject gameobject) = 0;
	virtual void setJointConnectedBody(GameObject gameobject, GameObject connected_body) = 0;
	virtual Vec3 getJointAxisPosition(GameObject gameobject) = 0;
	virtual void setJointAxisPosition(GameObject gameobject, const Vec3& value) = 0;
	virtual Vec3 getJointAxisDirection(GameObject gameobject) = 0;
	virtual void setJointAxisDirection(GameObject gameobject, const Vec3& value) = 0;
	virtual RigidTransform getJointLocalFrame(GameObject gameobject) = 0;
	virtual RigidTransform getJointConnectedBodyLocalFrame(GameObject gameobject) = 0;
	virtual physx::PxJoint* getJoint(GameObject gameobject) = 0;

	virtual bool getSphericalJointUseLimit(GameObject gameobject) = 0;
	virtual void setSphericalJointUseLimit(GameObject gameobject, bool use_limit) = 0;
	virtual Vec2 getSphericalJointLimit(GameObject gameobject) = 0;
	virtual void setSphericalJointLimit(GameObject gameobject, const Vec2& limit) = 0;

	virtual void applyForceToActor(GameObject gameobject, const Vec3& force) = 0;
	virtual void applyImpulseToActor(GameObject gameobject, const Vec3& force) = 0;
	virtual Vec3 getActorVelocity(GameObject gameobject) = 0;
	virtual float getActorSpeed(GameObject gameobject) = 0;
	virtual void putToSleep(GameObject gameobject) = 0;

	virtual bool isControllerCollisionDown(GameObject gameobject) const = 0;
	virtual void moveController(GameObject gameobject, const Vec3& v) = 0;
	virtual int getControllerLayer(GameObject gameobject) = 0;
	virtual void setControllerLayer(GameObject gameobject, int layer) = 0;
	virtual float getControllerRadius(GameObject gameobject) = 0;
	virtual void setControllerRadius(GameObject gameobject, float radius) = 0;
	virtual float getControllerHeight(GameObject gameobject) = 0;
	virtual void setControllerHeight(GameObject gameobject, float height) = 0;
	virtual bool getControllerCustomGravity(GameObject gameobject) = 0;
	virtual void setControllerCustomGravity(GameObject gameobject, bool gravity) = 0;
	virtual float getControllerCustomGravityAcceleration(GameObject gameobject) = 0;
	virtual void setControllerCustomGravityAcceleration(GameObject gameobject, float gravityacceleration) = 0;
	virtual bool isControllerTouchingDown(GameObject gameobject) = 0;
	virtual void resizeController(GameObject gameobject, float height) = 0;

	virtual void addBoxGeometry(GameObject gameobject, int index) = 0;
	virtual void removeBoxGeometry(GameObject gameobject, int index) = 0;
	virtual int getBoxGeometryCount(GameObject gameobject) = 0;
	virtual Vec3 getBoxGeomHalfExtents(GameObject gameobject, int index) = 0;
	virtual void setBoxGeomHalfExtents(GameObject gameobject, int index, const Vec3& size) = 0;
	virtual Vec3 getBoxGeomOffsetPosition(GameObject gameobject, int index) = 0;
	virtual void setBoxGeomOffsetPosition(GameObject gameobject, int index, const Vec3& pos) = 0;
	virtual Vec3 getBoxGeomOffsetRotation(GameObject gameobject, int index) = 0;
	virtual void setBoxGeomOffsetRotation(GameObject gameobject, int index, const Vec3& euler_angles) = 0;

	virtual void addSphereGeometry(GameObject gameobject, int index) = 0;
	virtual void removeSphereGeometry(GameObject gameobject, int index) = 0;
	virtual int getSphereGeometryCount(GameObject gameobject) = 0;
	virtual float getSphereGeomRadius(GameObject gameobject, int index) = 0;
	virtual void setSphereGeomRadius(GameObject gameobject, int index, float size) = 0;
	virtual Vec3 getSphereGeomOffsetPosition(GameObject gameobject, int index) = 0;
	virtual void setSphereGeomOffsetPosition(GameObject gameobject, int index, const Vec3& pos) = 0;
	virtual Vec3 getSphereGeomOffsetRotation(GameObject gameobject, int index) = 0;
	virtual void setSphereGeomOffsetRotation(GameObject gameobject, int index, const Vec3& euler_angles) = 0;

	virtual BoneOrientation getNewBoneOrientation() const = 0;
	virtual void setNewBoneOrientation(BoneOrientation orientation) = 0;
	virtual RagdollBone* createRagdollBone(GameObject gameobject, u32 bone_name_hash) = 0;
	virtual void destroyRagdollBone(GameObject gameobject, RagdollBone* bone) = 0;
	virtual physx::PxJoint* getRagdollBoneJoint(RagdollBone* bone) const = 0;
	virtual RagdollBone* getRagdollRootBone(GameObject gameobject) const = 0;
	virtual RagdollBone* getRagdollBoneChild(RagdollBone* bone) = 0;
	virtual RagdollBone* getRagdollBoneSibling(RagdollBone* bone) = 0;
	virtual RagdollBone* getRagdollBoneByName(GameObject gameobject, u32 bone_name_hash) = 0;
	virtual const char* getRagdollBoneName(RagdollBone* bone) = 0;
	virtual float getRagdollBoneHeight(RagdollBone* bone) = 0;
	virtual float getRagdollBoneRadius(RagdollBone* bone) = 0;
	virtual void setRagdollBoneHeight(RagdollBone* bone, float value) = 0;
	virtual void setRagdollBoneRadius(RagdollBone* bone, float value) = 0;
	virtual RigidTransform getRagdollBoneTransform(RagdollBone* bone) = 0;
	virtual void setRagdollBoneTransform(RagdollBone* bone, const RigidTransform& matrix) = 0;
	virtual void changeRagdollBoneJoint(RagdollBone* child, int type) = 0;
	virtual void getRagdollData(GameObject gameobject, OutputBlob& blob) = 0;
	virtual void setRagdollData(GameObject gameobject, InputBlob& blob) = 0;
	virtual void setRagdollBoneKinematicRecursive(RagdollBone* bone, bool is_kinematic) = 0;
	virtual void setRagdollBoneKinematic(RagdollBone* bone, bool is_kinematic) = 0;
	virtual bool isRagdollBoneKinematic(RagdollBone* bone) = 0;
	virtual void setRagdollLayer(GameObject gameobject, int layer) = 0;
	virtual int getRagdollLayer(GameObject gameobject) = 0;

	virtual const char* getCollisionLayerName(int index) = 0;
	virtual void setCollisionLayerName(int index, const char* name) = 0;
	virtual bool canLayersCollide(int layer1, int layer2) = 0;
	virtual void setLayersCanCollide(int layer1, int layer2, bool can_collide) = 0;
	virtual int getCollisionsLayersCount() const = 0;
	virtual void addCollisionLayer() = 0;
	virtual void removeCollisionLayer() = 0;

	virtual u32 getDebugVisualizationFlags() const = 0;
	virtual void setDebugVisualizationFlags(u32 flags) = 0;
	virtual void setVisualizationCullingBox(const Vec3& min, const Vec3& max) = 0;

	virtual int getActorCount() const = 0;
	virtual GameObject getActorGameObject(int index) = 0;
	virtual ActorType getActorType(int index) = 0;
	virtual bool isActorDebugEnabled(int index) const = 0;
	virtual void enableActorDebug(int index, bool enable) const = 0;
};


} // namespace Malmy

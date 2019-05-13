#pragma once


#include "engine/malmy.h"
#include "engine/flag_set.h"
#include "engine/matrix.h"
#include "engine/iplugin.h"


struct lua_State;


namespace Malmy
{

struct AABB;
class Engine;
struct Frustum;
struct IAllocator;
class LIFOAllocator;
class Material;
struct Mesh;
class Model;
class Path;
struct Pose;
struct RayCastModelHit;
class Renderer;
class Shader;
class Terrain;
class Texture;
class Project;
template <typename T> class Array;
template <typename T, typename T2> class AssociativeArray;
template <typename T> class DelegateList;


struct TerrainInfo
{
	Matrix m_world_matrix;
	Shader* m_shader;
	Terrain* m_terrain;
	Vec3 m_morph_const;
	float m_size;
	Vec3 m_min;
	int m_index;
};


struct DecalInfo
{
	Matrix mtx;
	Matrix inv_mtx;
	Material* material;
	Vec3 position;
	float radius;
};


struct ModelInstance
{
	enum Flags : u8
	{
		CUSTOM_MESHES = 1 << 0,
		KEEP_SKIN_DEPRECATED = 1 << 1,
		IS_BONE_ATTACHMENT_PARENT = 1 << 2,
		ENABLED = 1 << 3,

		RUNTIME_FLAGS = CUSTOM_MESHES,
		PERSISTENT_FLAGS = u8(~RUNTIME_FLAGS)
	};

	Matrix matrix;
	Model* model;
	Pose* pose;
	GameObject gameobject;
	Mesh* meshes;
	FlagSet<Flags, u8> flags;
	i8 mesh_count;
};


struct MeshInstance
{
	GameObject owner;
	Mesh* mesh;
	float depth;
};


struct GrassInfo
{
	struct InstanceData
	{
		Vec4 pos_scale;
		Quat rot;
		Vec4 normal;
	};
	Model* model;
	const InstanceData* instance_data;
	int instance_count;
	float type_distance;
};


struct DebugTriangle
{
	Vec3 p0;
	Vec3 p1;
	Vec3 p2;
	u32 color;
	float life;
};


struct DebugLine
{
	Vec3 from;
	Vec3 to;
	u32 color;
	float life;
};


struct DebugPoint
{
	Vec3 pos;
	u32 color;
	float life;
};


struct TextMeshVertex
{
	Vec3 pos;
	u32 color;
	Vec2 tex_coord;
};


class MALMY_RENDERER_API RenderScene : public IScene
{
public:
	static RenderScene* createInstance(Renderer& renderer,
		Engine& engine,
		Project& project,
		IAllocator& allocator);
	static void destroyInstance(RenderScene* scene);
	static void registerLuaAPI(lua_State* L);

	virtual RayCastModelHit castRay(const Vec3& origin, const Vec3& dir, GameObject ignore) = 0;
	virtual RayCastModelHit castRayTerrain(GameObject gameobject, const Vec3& origin, const Vec3& dir) = 0;
	virtual void getRay(GameObject gameobject, const Vec2& screen_pos, Vec3& origin, Vec3& dir) = 0;

	virtual Frustum getCameraFrustum(GameObject gameobject) const = 0;
	virtual Frustum getCameraFrustum(GameObject gameobject, const Vec2& a, const Vec2& b) const = 0;
	virtual float getTime() const = 0;
	virtual Engine& getEngine() const = 0;
	virtual IAllocator& getAllocator() = 0;

	virtual Pose* lockPose(GameObject gameobject) = 0;
	virtual void unlockPose(GameObject gameobject, bool changed) = 0;
	virtual GameObject getActiveGlobalLight() = 0;
	virtual void setActiveGlobalLight(GameObject gameobject) = 0;
	virtual Vec4 getShadowmapCascades(GameObject gameobject) = 0;
	virtual void setShadowmapCascades(GameObject gameobject, const Vec4& value) = 0;

	virtual void addDebugTriangle(const Vec3& p0,
		const Vec3& p1,
		const Vec3& p2,
		u32 color,
		float life) = 0;
	virtual void addDebugPoint(const Vec3& pos, u32 color, float life) = 0;
	virtual void addDebugCone(const Vec3& vertex,
		const Vec3& dir,
		const Vec3& axis0,
		const Vec3& axis1,
		u32 color,
		float life) = 0;

	virtual void addDebugLine(const Vec3& from, const Vec3& to, u32 color, float life) = 0;
	virtual void addDebugCross(const Vec3& center, float size, u32 color, float life) = 0;
	virtual void addDebugCube(const Vec3& pos,
		const Vec3& dir,
		const Vec3& up,
		const Vec3& right,
		u32 color,
		float life) = 0;
	virtual void addDebugCube(const Vec3& from, const Vec3& max, u32 color, float life) = 0;
	virtual void addDebugCubeSolid(const Vec3& from, const Vec3& max, u32 color, float life) = 0;
	virtual void addDebugCircle(const Vec3& center,
		const Vec3& up,
		float radius,
		u32 color,
		float life) = 0;
	virtual void addDebugSphere(const Vec3& center, float radius, u32 color, float life) = 0;
	virtual void addDebugFrustum(const Frustum& frustum, u32 color, float life) = 0;

	virtual void addDebugCapsule(const Vec3& position,
		float height,
		float radius,
		u32 color,
		float life) = 0;

	virtual void addDebugCapsule(const Matrix& transform,
		float height,
		float radius,
		u32 color,
		float life) = 0;

	virtual void addDebugCylinder(const Vec3& position,
		const Vec3& up,
		float radius,
		u32 color,
		float life) = 0;

	virtual GameObject getBoneAttachmentParent(GameObject gameobject) = 0;
	virtual void setBoneAttachmentParent(GameObject gameobject, GameObject parent) = 0;
	virtual void setBoneAttachmentBone(GameObject gameobject, int value) = 0;
	virtual int getBoneAttachmentBone(GameObject gameobject) = 0;
	virtual Vec3 getBoneAttachmentPosition(GameObject gameobject) = 0;
	virtual void setBoneAttachmentPosition(GameObject gameobject, const Vec3& pos) = 0;
	virtual Vec3 getBoneAttachmentRotation(GameObject gameobject) = 0;
	virtual void setBoneAttachmentRotation(GameObject gameobject, const Vec3& rot) = 0;
	virtual void setBoneAttachmentRotationQuat(GameObject gameobject, const Quat& rot) = 0;

	virtual const Array<DebugTriangle>& getDebugTriangles() const = 0;
	virtual const Array<DebugLine>& getDebugLines() const = 0;
	virtual const Array<DebugPoint>& getDebugPoints() const = 0;

	virtual Matrix getCameraProjection(GameObject gameobject) = 0;
	virtual Matrix getCameraViewProjection(GameObject gameobject) = 0;
	virtual GameObject getCameraInSlot(const char* slot) = 0;
	virtual float getCameraFOV(GameObject gameobject) = 0;
	virtual void setCameraFOV(GameObject gameobject, float fov) = 0;
	virtual void setCameraFarPlane(GameObject gameobject, float far) = 0;
	virtual void setCameraNearPlane(GameObject gameobject, float near) = 0;
	virtual float getCameraFarPlane(GameObject gameobject) = 0;
	virtual float getCameraNearPlane(GameObject gameobject) = 0;
	virtual float getCameraScreenWidth(GameObject gameobject) = 0;
	virtual float getCameraScreenHeight(GameObject gameobject) = 0;
	virtual void setCameraSlot(GameObject gameobject, const char* slot) = 0;
	virtual const char* getCameraSlot(GameObject gameobject) = 0;
	virtual void setCameraScreenSize(GameObject gameobject, int w, int h) = 0;
	virtual bool isCameraOrtho(GameObject gameobject) = 0;
	virtual void setCameraOrtho(GameObject gameobject, bool is_ortho) = 0;
	virtual float getCameraOrthoSize(GameObject gameobject) = 0;
	virtual void setCameraOrthoSize(GameObject gameobject, float value) = 0;
	virtual Vec2 getCameraScreenSize(GameObject gameobject) = 0;

	virtual void setScriptedParticleEmitterMaterialPath(GameObject gameobject, const Path& path) = 0;
	virtual Path getScriptedParticleEmitterMaterialPath(GameObject gameobject) = 0;
	virtual const AssociativeArray<GameObject, class ScriptedParticleEmitter*>& getScriptedParticleEmitters() const = 0;

	virtual class ParticleEmitter* getParticleEmitter(GameObject gameobject) = 0;
	virtual void resetParticleEmitter(GameObject gameobject) = 0;
	virtual void updateEmitter(GameObject gameobject, float time_delta) = 0;
	virtual const AssociativeArray<GameObject, class ParticleEmitter*>& getParticleEmitters() const = 0;
	virtual const Vec2* getParticleEmitterAlpha(GameObject gameobject) = 0;
	virtual int getParticleEmitterAlphaCount(GameObject gameobject) = 0;
	virtual const Vec2* getParticleEmitterSize(GameObject gameobject) = 0;
	virtual int getParticleEmitterSizeCount(GameObject gameobject) = 0;
	virtual bool getParticleEmitterAutoemit(GameObject gameobject) = 0;
	virtual bool getParticleEmitterLocalSpace(GameObject gameobject) = 0;
	virtual Vec3 getParticleEmitterAcceleration(GameObject gameobject) = 0;
	virtual Vec2 getParticleEmitterLinearMovementX(GameObject gameobject) = 0;
	virtual Vec2 getParticleEmitterLinearMovementY(GameObject gameobject) = 0;
	virtual Vec2 getParticleEmitterLinearMovementZ(GameObject gameobject) = 0;
	virtual Vec2 getParticleEmitterInitialLife(GameObject gameobject) = 0;
	virtual Int2 getParticleEmitterSpawnCount(GameObject gameobject) = 0;
	virtual Vec2 getParticleEmitterSpawnPeriod(GameObject gameobject) = 0;
	virtual Vec2 getParticleEmitterInitialSize(GameObject gameobject) = 0;
	virtual void setParticleEmitterAutoemit(GameObject gameobject, bool autoemit) = 0;
	virtual void setParticleEmitterLocalSpace(GameObject gameobject, bool autoemit) = 0;
	virtual void setParticleEmitterAlpha(GameObject gameobject, const Vec2* value, int count) = 0;
	virtual void setParticleEmitterSize(GameObject gameobject, const Vec2* values, int count) = 0;
	virtual void setParticleEmitterAcceleration(GameObject gameobject, const Vec3& value) = 0;
	virtual void setParticleEmitterLinearMovementX(GameObject gameobject, const Vec2& value) = 0;
	virtual void setParticleEmitterLinearMovementY(GameObject gameobject, const Vec2& value) = 0;
	virtual void setParticleEmitterLinearMovementZ(GameObject gameobject, const Vec2& value) = 0;
	virtual void setParticleEmitterInitialLife(GameObject gameobject, const Vec2& value) = 0;
	virtual void setParticleEmitterSpawnCount(GameObject gameobject, const Int2& value) = 0;
	virtual void setParticleEmitterSpawnPeriod(GameObject gameobject, const Vec2& value) = 0;
	virtual void setParticleEmitterInitialSize(GameObject gameobject, const Vec2& value) = 0;
	virtual void setParticleEmitterMaterialPath(GameObject gameobject, const Path& path) = 0;
	virtual void setParticleEmitterSubimageRows(GameObject gameobject, const int& value) = 0;
	virtual void setParticleEmitterSubimageCols(GameObject gameobject, const int& value) = 0;
	virtual Path getParticleEmitterMaterialPath(GameObject gameobject) = 0;
	virtual int getParticleEmitterPlaneCount(GameObject gameobject) = 0;
	virtual int getParticleEmitterSubimageRows(GameObject gameobject) = 0;
	virtual int getParticleEmitterSubimageCols(GameObject gameobject) = 0;
	virtual void addParticleEmitterPlane(GameObject gameobject, int index) = 0;
	virtual void removeParticleEmitterPlane(GameObject gameobject, int index) = 0;
	virtual GameObject getParticleEmitterPlaneGameObject(GameObject gameobject, int index) = 0;
	virtual void setParticleEmitterPlaneGameObject(GameObject module, int index, GameObject gameobject) = 0;
	virtual float getParticleEmitterPlaneBounce(GameObject gameobject) = 0;
	virtual void setParticleEmitterPlaneBounce(GameObject gameobject, float value) = 0;
	virtual float getParticleEmitterShapeRadius(GameObject gameobject) = 0;
	virtual void setParticleEmitterShapeRadius(GameObject gameobject, float value) = 0;

	virtual int getParticleEmitterAttractorCount(GameObject gameobject) = 0;
	virtual void addParticleEmitterAttractor(GameObject gameobject, int index) = 0;
	virtual void removeParticleEmitterAttractor(GameObject gameobject, int index) = 0;
	virtual GameObject getParticleEmitterAttractorGameObject(GameObject gameobject, int index) = 0;
	virtual void setParticleEmitterAttractorGameObject(GameObject module,
		int index,
		GameObject gameobject) = 0;
	virtual float getParticleEmitterAttractorForce(GameObject gameobject) = 0;
	virtual void setParticleEmitterAttractorForce(GameObject gameobject, float value) = 0;

	virtual void enableModelInstance(GameObject gameobject, bool enable) = 0;
	virtual bool isModelInstanceEnabled(GameObject gameobject) = 0;
	virtual ModelInstance* getModelInstance(GameObject gameobject) = 0;
	virtual ModelInstance* getModelInstances() = 0;
	virtual Path getModelInstancePath(GameObject gameobject) = 0;
	virtual void setModelInstanceMaterial(GameObject gameobject, int index, const Path& path) = 0;
	virtual Path getModelInstanceMaterial(GameObject gameobject, int index) = 0;
	virtual int getModelInstanceMaterialsCount(GameObject gameobject) = 0;
	virtual void setModelInstancePath(GameObject gameobject, const Path& path) = 0;
	virtual Array<Array<MeshInstance>>& getModelInstanceInfos(const Frustum& frustum,
		const Vec3& lod_ref_point,
		GameObject gameobject,
		u64 layer_mask) = 0;
	virtual void getModelInstanceEntities(const Frustum& frustum, Array<GameObject>& entities) = 0;
	virtual GameObject getFirstModelInstance() = 0;
	virtual GameObject getNextModelInstance(GameObject gameobject) = 0;
	virtual Model* getModelInstanceModel(GameObject gameobject) = 0;

	virtual void setDecalMaterialPath(GameObject gameobject, const Path& path) = 0;
	virtual Path getDecalMaterialPath(GameObject gameobject) = 0;
	virtual void setDecalScale(GameObject gameobject, const Vec3& value) = 0;
	virtual Vec3 getDecalScale(GameObject gameobject) = 0;
	virtual void getDecals(const Frustum& frustum, Array<DecalInfo>& decals) = 0;

	virtual void getGrassInfos(const Frustum& frustum,
		GameObject gameobject,
		Array<GrassInfo>& infos) = 0;
	virtual void forceGrassUpdate(GameObject gameobject) = 0;
	virtual void getTerrainInfos(const Frustum& frustum, const Vec3& lod_ref_point, Array<TerrainInfo>& infos) = 0;
	virtual float getTerrainHeightAt(GameObject gameobject, float x, float z) = 0;
	virtual Vec3 getTerrainNormalAt(GameObject gameobject, float x, float z) = 0;
	virtual void setTerrainMaterialPath(GameObject gameobject, const Path& path) = 0;
	virtual Path getTerrainMaterialPath(GameObject gameobject) = 0;
	virtual Material* getTerrainMaterial(GameObject gameobject) = 0;
	virtual void setTerrainXZScale(GameObject gameobject, float scale) = 0;
	virtual float getTerrainXZScale(GameObject gameobject) = 0;
	virtual void setTerrainYScale(GameObject gameobject, float scale) = 0;
	virtual float getTerrainYScale(GameObject gameobject) = 0;
	virtual Vec2 getTerrainSize(GameObject gameobject) = 0;
	virtual AABB getTerrainAABB(GameObject gameobject) = 0;
	virtual GameObject getTerrainGameObject(GameObject gameobject) = 0;
	virtual Vec2 getTerrainResolution(GameObject gameobject) = 0;
	virtual GameObject getFirstTerrain() = 0;
	virtual GameObject getNextTerrain(GameObject gameobject) = 0;

	virtual bool isGrassEnabled() const = 0;
	virtual int getGrassRotationMode(GameObject gameobject, int index) = 0;
	virtual void setGrassRotationMode(GameObject gameobject, int index, int value) = 0;
	virtual float getGrassDistance(GameObject gameobject, int index) = 0;
	virtual void setGrassDistance(GameObject gameobject, int index, float value) = 0;
	virtual void enableGrass(bool enabled) = 0;
	virtual void setGrassPath(GameObject gameobject, int index, const Path& path) = 0;
	virtual Path getGrassPath(GameObject gameobject, int index) = 0;
	virtual void setGrassDensity(GameObject gameobject, int index, int density) = 0;
	virtual int getGrassDensity(GameObject gameobject, int index) = 0;
	virtual int getGrassCount(GameObject gameobject) = 0;
	virtual void addGrass(GameObject gameobject, int index) = 0;
	virtual void removeGrass(GameObject gameobject, int index) = 0;

	virtual int getClosestPointLights(const Vec3& pos, GameObject* lights, int max_lights) = 0;
	virtual void getPointLights(const Frustum& frustum, Array<GameObject>& lights) = 0;
	virtual void getPointLightInfluencedGeometry(GameObject light,
		GameObject camera,
		const Vec3& lod_ref_point, 
		Array<MeshInstance>& infos) = 0;
	virtual void getPointLightInfluencedGeometry(GameObject light,
		GameObject camera,
		const Vec3& lod_ref_point,
		const Frustum& frustum,
		Array<MeshInstance>& infos) = 0;
	virtual void setLightCastShadows(GameObject gameobject, bool cast_shadows) = 0;
	virtual bool getLightCastShadows(GameObject gameobject) = 0;
	virtual float getLightAttenuation(GameObject gameobject) = 0;
	virtual void setLightAttenuation(GameObject gameobject, float attenuation) = 0;
	virtual float getLightFOV(GameObject gameobject) = 0;
	virtual void setLightFOV(GameObject gameobject, float fov) = 0;
	virtual float getLightRange(GameObject gameobject) = 0;
	virtual void setLightRange(GameObject gameobject, float value) = 0;
	virtual void setPointLightIntensity(GameObject gameobject, float intensity) = 0;
	virtual void setGlobalLightIntensity(GameObject gameobject, float intensity) = 0;
	virtual void setGlobalLightIndirectIntensity(GameObject gameobject, float intensity) = 0;
	virtual void setPointLightColor(GameObject gameobject, const Vec3& color) = 0;
	virtual void setGlobalLightColor(GameObject gameobject, const Vec3& color) = 0;
	virtual void setFogDensity(GameObject gameobject, float density) = 0;
	virtual void setFogColor(GameObject gameobject, const Vec3& color) = 0;
	virtual float getPointLightIntensity(GameObject gameobject) = 0;
	virtual GameObject getPointLightGameObject(GameObject gameobject) const = 0;
	virtual GameObject getGlobalLightGameObject(GameObject gameobject) const = 0;
	virtual float getGlobalLightIntensity(GameObject gameobject) = 0;
	virtual float getGlobalLightIndirectIntensity(GameObject gameobject) = 0;
	virtual Vec3 getPointLightColor(GameObject gameobject) = 0;
	virtual Vec3 getGlobalLightColor(GameObject gameobject) = 0;
	virtual float getFogDensity(GameObject gameobject) = 0;
	virtual float getFogBottom(GameObject gameobject) = 0;
	virtual float getFogHeight(GameObject gameobject) = 0;
	virtual void setFogBottom(GameObject gameobject, float value) = 0;
	virtual void setFogHeight(GameObject gameobject, float value) = 0;
	virtual Vec3 getFogColor(GameObject gameobject) = 0;
	virtual Vec3 getPointLightSpecularColor(GameObject gameobject) = 0;
	virtual void setPointLightSpecularColor(GameObject gameobject, const Vec3& color) = 0;
	virtual float getPointLightSpecularIntensity(GameObject gameobject) = 0;
	virtual void setPointLightSpecularIntensity(GameObject gameobject, float color) = 0;

	virtual int getEnvironmentProbeIrradianceSize(GameObject gameobject) = 0;
	virtual void setEnvironmentProbeIrradianceSize(GameObject gameobject, int size) = 0;
	virtual int getEnvironmentProbeRadianceSize(GameObject gameobject) = 0;
	virtual void setEnvironmentProbeRadianceSize(GameObject gameobject, int size) = 0;
	virtual int getEnvironmentProbeReflectionSize(GameObject gameobject) = 0;
	virtual void setEnvironmentProbeReflectionSize(GameObject gameobject, int size) = 0;
	virtual bool isEnvironmentProbeReflectionEnabled(GameObject gameobject) = 0;
	virtual void enableEnvironmentProbeReflection(GameObject gameobject, bool enable) = 0;
	virtual bool isEnvironmentProbeCustomSize(GameObject gameobject) = 0;
	virtual void enableEnvironmentProbeCustomSize(GameObject gameobject, bool enable) = 0;
	virtual Texture* getEnvironmentProbeTexture(GameObject gameobject) const = 0;
	virtual Texture* getEnvironmentProbeIrradiance(GameObject gameobject) const = 0;
	virtual Texture* getEnvironmentProbeRadiance(GameObject gameobject) const = 0;
	virtual void reloadEnvironmentProbe(GameObject gameobject) = 0;
	virtual GameObject getNearestEnvironmentProbe(const Vec3& pos) const = 0;
	virtual u64 getEnvironmentProbeGUID(GameObject gameobject) const = 0;

	virtual void setTextMeshText(GameObject gameobject, const char* text) = 0;
	virtual const char* getTextMeshText(GameObject gameobject) = 0;
	virtual void setTextMeshFontSize(GameObject gameobject, int value) = 0;
	virtual int getTextMeshFontSize(GameObject gameobject) = 0;
	virtual Vec4 getTextMeshColorRGBA(GameObject gameobject) = 0;
	virtual void setTextMeshColorRGBA(GameObject gameobject, const Vec4& color) = 0;
	virtual Path getTextMeshFontPath(GameObject gameobject) = 0;
	virtual void setTextMeshFontPath(GameObject gameobject, const Path& path) = 0;
	virtual bool isTextMeshCameraOriented(GameObject gameobject) = 0;
	virtual void setTextMeshCameraOriented(GameObject gameobject, bool is_oriented) = 0;
	virtual void getTextMeshesVertices(Array<TextMeshVertex>& vertices, GameObject camera) = 0;

protected:
	virtual ~RenderScene() {}
};
}
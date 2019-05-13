#pragma once


#include "engine/malmy.h"


namespace Malmy
{


struct IDeserializer;
class InputBlob;
struct ISerializer;
class OutputBlob;
class Path;
struct PrefabResource;
struct Quat;
struct Vec3;
class WorldEditor;
class StudioApp;


class MALMY_EDITOR_API PrefabSystem
{
public:
	static PrefabSystem* create(WorldEditor& editor);
	static void destroy(PrefabSystem* system);
	static void createAssetBrowserPlugin(StudioApp& app, PrefabSystem& system);
	static void destroyAssetBrowserPlugin(StudioApp& app);

	virtual ~PrefabSystem() {}
	virtual void serialize(OutputBlob& serializer) = 0;
	virtual void deserialize(InputBlob& serializer) = 0;
	virtual void serialize(ISerializer& serializer) = 0;
	virtual void deserialize(IDeserializer& serializer) = 0;
	virtual GameObject instantiatePrefab(PrefabResource& prefab, const Vec3& pos, const Quat& rot, float scale) = 0;
	virtual u64 getPrefab(GameObject gameobject) const = 0;
	virtual int getMaxGameObjectIndex() const = 0;
	virtual void setPrefab(GameObject gameobject, u64 prefab) = 0;
	virtual GameObject getFirstInstance(u64 prefab) = 0;
	virtual GameObject getNextInstance(GameObject gameobject) = 0;
	virtual void savePrefab(const Path& path) = 0;
	virtual PrefabResource* getPrefabResource(GameObject gameobject) = 0;
};


} // namespace Malmy
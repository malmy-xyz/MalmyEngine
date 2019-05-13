#pragma once


#include "engine/array.h"
#include "engine/delegate_list.h"
#include "engine/iplugin.h"
#include "engine/malmy.h"
#include "engine/matrix.h"
#include "engine/quat.h"
#include "engine/string.h"


namespace Malmy
{


struct ComponentUID;
struct IDeserializer;
class InputBlob;
struct IScene;
struct ISerializer;
class OutputBlob;
struct PrefabResource;


class MALMY_ENGINE_API Project
{
public:
	typedef void (IScene::*Create)(GameObject);
	typedef void (IScene::*Destroy)(GameObject);
	typedef void (IScene::*Serialize)(ISerializer&, GameObject);
	typedef void (IScene::*Deserialize)(IDeserializer&, GameObject, int);
	struct ComponentTypeEntry
	{
		IScene* scene = nullptr;
		void (IScene::*create)(GameObject);
		void (IScene::*destroy)(GameObject);
		void (IScene::*serialize)(ISerializer&, GameObject);
		void (IScene::*deserialize)(IDeserializer&, GameObject, int);
	};

	enum { GAMEOBJECT_NAME_MAX_LENGTH = 32 };

public:
	explicit Project(IAllocator& allocator);
	~Project();

	IAllocator& getAllocator() { return m_allocator; }
	void emplaceGameObject(GameObject gameobject);
	GameObject createGameObject(const Vec3& position, const Quat& rotation);
	GameObject cloneGameObject(GameObject gameobject);
	void destroyGameObject(GameObject gameobject);
	void createComponent(ComponentType type, GameObject gameobject);
	void destroyComponent(GameObject gameobject, ComponentType type);
	void onComponentCreated(GameObject gameobject, ComponentType component_type, IScene* scene);
	void onComponentDestroyed(GameObject gameobject, ComponentType component_type, IScene* scene);
	bool hasComponent(GameObject gameobject, ComponentType component_type) const;
	ComponentUID getComponent(GameObject gameobject, ComponentType type) const;
	ComponentUID getFirstComponent(GameObject gameobject) const;
	ComponentUID getNextComponent(const ComponentUID& cmp) const;
	ComponentTypeEntry& registerComponentType(ComponentType type) { return m_component_type_map[type.index]; }
	template <typename T1, typename T2, typename T3, typename T4>
	void registerComponentType(ComponentType type, IScene* scene, T1 create, T2 destroy, T3 serialize, T4 deserialize)
	{
		m_component_type_map[type.index].scene = scene;
		m_component_type_map[type.index].create = static_cast<Create>(create);
		m_component_type_map[type.index].destroy = static_cast<Destroy>(destroy);
		m_component_type_map[type.index].serialize = static_cast<Serialize>(serialize);
		m_component_type_map[type.index].deserialize = static_cast<Deserialize>(deserialize);
	}

	GameObject getFirstGameObject() const;
	GameObject getNextGameObject(GameObject gameobject) const;
	const char* getGameObjectName(GameObject gameobject) const;
	GameObject findByName(GameObject parent, const char* name);
	void setGameObjectName(GameObject gameobject, const char* name);
	bool hasGameObject(GameObject gameobject) const;

	bool isDescendant(GameObject ancestor, GameObject descendant) const;
	GameObject getParent(GameObject gameobject) const;
	GameObject getFirstChild(GameObject gameobject) const;
	GameObject getNextSibling(GameObject gameobject) const;
	Transform getLocalTransform(GameObject gameobject) const;
	float getLocalScale(GameObject gameobject) const;
	void setParent(GameObject parent, GameObject child);
	void setLocalPosition(GameObject gameobject, const Vec3& pos);
	void setLocalRotation(GameObject gameobject, const Quat& rot);
	void setLocalTransform(GameObject gameobject, const Transform& transform);
	Transform computeLocalTransform(GameObject parent, const Transform& global_transform) const;

	void setMatrix(GameObject gameobject, const Matrix& mtx);
	Matrix getPositionAndRotation(GameObject gameobject) const;
	Matrix getMatrix(GameObject gameobject) const;
	void setTransform(GameObject gameobject, const RigidTransform& transform);
	void setTransform(GameObject gameobject, const Transform& transform);
	void setTransformKeepChildren(GameObject gameobject, const Transform& transform);
	void setTransform(GameObject gameobject, const Vec3& pos, const Quat& rot, float scale);
	Transform getTransform(GameObject gameobject) const;
	void setRotation(GameObject gameobject, float x, float y, float z, float w);
	void setRotation(GameObject gameobject, const Quat& rot);
	void setPosition(GameObject gameobject, float x, float y, float z);
	void setPosition(GameObject gameobject, const Vec3& pos);
	void setScale(GameObject gameobject, float scale);
	GameObject instantiatePrefab(const PrefabResource& prefab,
		const Vec3& pos,
		const Quat& rot,
		float scale);
	float getScale(GameObject gameobject) const;
	const Vec3& getPosition(GameObject gameobject) const;
	const Quat& getRotation(GameObject gameobject) const;
	const char* getName() const { return m_name; }
	void setName(const char* name) 
	{ 
		m_name = name; 
	}

	DelegateList<void(GameObject)>& gameobjectTransformed() { return m_gameobject_moved; }
	DelegateList<void(GameObject)>& gameobjectCreated() { return m_gameobject_created; }
	DelegateList<void(GameObject)>& gameobjectDestroyed() { return m_gameobject_destroyed; }
	DelegateList<void(const ComponentUID&)>& componentDestroyed() { return m_component_destroyed; }
	DelegateList<void(const ComponentUID&)>& componentAdded() { return m_component_added; }

	void serializeComponent(ISerializer& serializer, ComponentType type, GameObject gameobject);
	void deserializeComponent(IDeserializer& serializer, GameObject gameobject, ComponentType type, int scene_version);
	void serialize(OutputBlob& serializer);
	void deserialize(InputBlob& serializer);

	IScene* getScene(ComponentType type) const;
	IScene* getScene(u32 hash) const;
	Array<IScene*>& getScenes();
	void addScene(IScene* scene);
	void removeScene(IScene* scene);

private:
	void transformGameObject(GameObject gameobject, bool update_local);
	void updateGlobalTransform(GameObject gameobject);

	struct Hierarchy
	{
		GameObject gameobject;
		GameObject parent;
		GameObject first_child;
		GameObject next_sibling;

		Transform local_transform;
	};


	struct GameObjectData
	{
		GameObjectData() {}

		Vec3 position;
		Quat rotation;
		
		int hierarchy;
		int name;

		union
		{
			struct 
			{
				float scale;
				u64 components;
			};
			struct
			{
				int prev;
				int next;
			};
		};
		bool valid;
	};

	struct GameObjectName
	{
		GameObject gameobject;
		char name[GAMEOBJECT_NAME_MAX_LENGTH];
	};

private:
	IAllocator& m_allocator;
	ComponentTypeEntry m_component_type_map[ComponentType::MAX_TYPES_COUNT];
	Array<IScene*> m_scenes;
	Array<GameObjectData> m_entities;
	Array<Hierarchy> m_hierarchy;
	Array<GameObjectName> m_names;
	DelegateList<void(GameObject)> m_gameobject_moved;
	DelegateList<void(GameObject)> m_gameobject_created;
	DelegateList<void(GameObject)> m_gameobject_destroyed;
	DelegateList<void(const ComponentUID&)> m_component_destroyed;
	DelegateList<void(const ComponentUID&)> m_component_added;
	int m_first_free_slot;
	StaticString<64> m_name;
};


} // namespace Malmy

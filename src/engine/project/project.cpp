#include "project.h"
#include "engine/blob.h"
#include "engine/crc32.h"
#include "engine/iplugin.h"
#include "engine/log.h"
#include "engine/matrix.h"
#include "engine/prefab.h"
#include "engine/reflection.h"
#include "engine/serializer.h"
#include "engine/project/component.h"


namespace Malmy
{


static const int RESERVED_ENTITIES_COUNT = 5000;


Project::~Project() = default;


Project::Project(IAllocator& allocator)
	: m_allocator(allocator)
	, m_names(m_allocator)
	, m_entities(m_allocator)
	, m_component_added(m_allocator)
	, m_component_destroyed(m_allocator)
	, m_gameobject_created(m_allocator)
	, m_gameobject_destroyed(m_allocator)
	, m_gameobject_moved(m_allocator)
	, m_first_free_slot(-1)
	, m_scenes(m_allocator)
	, m_hierarchy(m_allocator)
{
	m_entities.reserve(RESERVED_ENTITIES_COUNT);
}


IScene* Project::getScene(ComponentType type) const
{
	return m_component_type_map[type.index].scene;
}


IScene* Project::getScene(u32 hash) const
{
	for (auto* scene : m_scenes)
	{
		if (crc32(scene->getPlugin().getName()) == hash)
		{
			return scene;
		}
	}
	return nullptr;
}


Array<IScene*>& Project::getScenes()
{
	return m_scenes;
}


void Project::addScene(IScene* scene)
{
	m_scenes.push(scene);
}


void Project::removeScene(IScene* scene)
{
	m_scenes.eraseItemFast(scene);
}


const Vec3& Project::getPosition(GameObject gameobject) const
{
	return m_entities[gameobject.index].position;
}


const Quat& Project::getRotation(GameObject gameobject) const
{
	return m_entities[gameobject.index].rotation;
}


void Project::transformGameObject(GameObject gameobject, bool update_local)
{
	int hierarchy_idx = m_entities[gameobject.index].hierarchy;
	gameobjectTransformed().invoke(gameobject);
	if (hierarchy_idx >= 0)
	{
		Hierarchy& h = m_hierarchy[hierarchy_idx];
		Transform my_transform = getTransform(gameobject);
		if (update_local && h.parent.isValid())
		{
			Transform parent_tr = getTransform(h.parent);
			h.local_transform = (parent_tr.inverted() * my_transform);
		}

		GameObject child = h.first_child;
		while (child.isValid())
		{
			Hierarchy& child_h = m_hierarchy[m_entities[child.index].hierarchy];
			Transform abs_tr = my_transform * child_h.local_transform;
			GameObjectData& child_data = m_entities[child.index];
			child_data.position = abs_tr.pos;
			child_data.rotation = abs_tr.rot;
			child_data.scale = abs_tr.scale;
			transformGameObject(child, false);

			child = child_h.next_sibling;
		}
	}
}


void Project::setRotation(GameObject gameobject, const Quat& rot)
{
	m_entities[gameobject.index].rotation = rot;
	transformGameObject(gameobject, true);
}


void Project::setRotation(GameObject gameobject, float x, float y, float z, float w)
{
	m_entities[gameobject.index].rotation.set(x, y, z, w);
	transformGameObject(gameobject, true);
}


bool Project::hasGameObject(GameObject gameobject) const
{
	return gameobject.index >= 0 && gameobject.index < m_entities.size() && m_entities[gameobject.index].valid;
}


void Project::setMatrix(GameObject gameobject, const Matrix& mtx)
{
	GameObjectData& out = m_entities[gameobject.index];
	mtx.decompose(out.position, out.rotation, out.scale);
	transformGameObject(gameobject, true);
}


Matrix Project::getPositionAndRotation(GameObject gameobject) const
{
	auto& transform = m_entities[gameobject.index];
	Matrix mtx = transform.rotation.toMatrix();
	mtx.setTranslation(transform.position);
	return mtx;
}


void Project::setTransformKeepChildren(GameObject gameobject, const Transform& transform)
{
	auto& tmp = m_entities[gameobject.index];
	tmp.position = transform.pos;
	tmp.rotation = transform.rot;
	tmp.scale = transform.scale;
	
	int hierarchy_idx = m_entities[gameobject.index].hierarchy;
	gameobjectTransformed().invoke(gameobject);
	if (hierarchy_idx >= 0)
	{
		Hierarchy& h = m_hierarchy[hierarchy_idx];
		Transform my_transform = getTransform(gameobject);
		if (h.parent.isValid())
		{
			Transform parent_tr = getTransform(h.parent);
			h.local_transform = parent_tr.inverted() * my_transform;
		}

		GameObject child = h.first_child;
		while (child.isValid())
		{
			Hierarchy& child_h = m_hierarchy[m_entities[child.index].hierarchy];

			child_h.local_transform = my_transform.inverted() * getTransform(child);
			child = child_h.next_sibling;
		}
	}

}


void Project::setTransform(GameObject gameobject, const Transform& transform)
{
	auto& tmp = m_entities[gameobject.index];
	tmp.position = transform.pos;
	tmp.rotation = transform.rot;
	tmp.scale = transform.scale;
	transformGameObject(gameobject, true);
}


void Project::setTransform(GameObject gameobject, const RigidTransform& transform)
{
	auto& tmp = m_entities[gameobject.index];
	tmp.position = transform.pos;
	tmp.rotation = transform.rot;
	transformGameObject(gameobject, true);
}


void Project::setTransform(GameObject gameobject, const Vec3& pos, const Quat& rot, float scale)
{
	auto& tmp = m_entities[gameobject.index];
	tmp.position = pos;
	tmp.rotation = rot;
	tmp.scale = scale;
	transformGameObject(gameobject, true);
}


Transform Project::getTransform(GameObject gameobject) const
{
	auto& transform = m_entities[gameobject.index];
	return {transform.position, transform.rotation, transform.scale};
}


Matrix Project::getMatrix(GameObject gameobject) const
{
	auto& transform = m_entities[gameobject.index];
	Matrix mtx = transform.rotation.toMatrix();
	mtx.setTranslation(transform.position);
	mtx.multiply3x3(transform.scale);
	return mtx;
}


void Project::setPosition(GameObject gameobject, float x, float y, float z)
{
	auto& transform = m_entities[gameobject.index];
	transform.position.set(x, y, z);
	transformGameObject(gameobject, true);
}


void Project::setPosition(GameObject gameobject, const Vec3& pos)
{
	auto& transform = m_entities[gameobject.index];
	transform.position = pos;
	transformGameObject(gameobject, true);
}


void Project::setGameObjectName(GameObject gameobject, const char* name)
{
	int name_idx = m_entities[gameobject.index].name;
	if (name_idx < 0)
	{
		if (name[0] == '\0') return;
		m_entities[gameobject.index].name = m_names.size();
		GameObjectName& name_data = m_names.emplace();
		name_data.gameobject = gameobject;
		copyString(name_data.name, name);
	}
	else
	{
		copyString(m_names[name_idx].name, name);
	}
}


const char* Project::getGameObjectName(GameObject gameobject) const
{
	int name_idx = m_entities[gameobject.index].name;
	if (name_idx < 0) return "";
	return m_names[name_idx].name;
}


GameObject Project::findByName(GameObject parent, const char* name)
{
	if (parent.isValid())
	{
		int h_idx = m_entities[parent.index].hierarchy;
		if (h_idx < 0) return INVALID_GAMEOBJECT;

		GameObject e = m_hierarchy[h_idx].first_child;
		while (e.isValid())
		{
			const GameObjectData& data = m_entities[e.index];
			int name_idx = data.name;
			if (name_idx >= 0)
			{
				if (equalStrings(m_names[name_idx].name, name)) return e;
			}
			e = m_hierarchy[data.hierarchy].next_sibling;
		}
	}
	else
	{
		for (int i = 0, c = m_names.size(); i < c; ++i)
		{
			if (equalStrings(m_names[i].name, name))
			{
				const GameObjectData& data = m_entities[m_names[i].gameobject.index];
				if (data.hierarchy < 0) return m_names[i].gameobject;
				if (!m_hierarchy[data.hierarchy].parent.isValid()) return m_names[i].gameobject;
			}
		}
	}

	return INVALID_GAMEOBJECT;
}


void Project::emplaceGameObject(GameObject gameobject)
{
	while (m_entities.size() <= gameobject.index)
	{
		GameObjectData& data = m_entities.emplace();
		data.valid = false;
		data.prev = -1;
		data.name = -1;
		data.hierarchy = -1;
		data.next = m_first_free_slot;
		data.scale = -1;
		if (m_first_free_slot >= 0)
		{
			m_entities[m_first_free_slot].prev = m_entities.size() - 1;
		}
		m_first_free_slot = m_entities.size() - 1;
	}
	if (m_first_free_slot == gameobject.index)
	{
		m_first_free_slot = m_entities[gameobject.index].next;
	}
	if (m_entities[gameobject.index].prev >= 0)
	{
		m_entities[m_entities[gameobject.index].prev].next = m_entities[gameobject.index].next;
	}
	if (m_entities[gameobject.index].next >= 0)
	{
		m_entities[m_entities[gameobject.index].next].prev= m_entities[gameobject.index].prev;
	}
	GameObjectData& data = m_entities[gameobject.index];
	data.position.set(0, 0, 0);
	data.rotation.set(0, 0, 0, 1);
	data.scale = 1;
	data.name = -1;
	data.hierarchy = -1;
	data.components = 0;
	data.valid = true;
	m_gameobject_created.invoke(gameobject);
}


GameObject Project::cloneGameObject(GameObject gameobject)
{
	Transform tr = getTransform(gameobject);
	GameObject parent = getParent(gameobject);
	GameObject clone = createGameObject(tr.pos, tr.rot);
	setScale(clone, tr.scale);
	setParent(parent, clone);

	OutputBlob blob_out(m_allocator);
	blob_out.reserve(1024);
	for (ComponentUID cmp = getFirstComponent(gameobject); cmp.isValid(); cmp = getNextComponent(cmp))
	{
		blob_out.clear();
		struct : ISaveGameObjectGUIDMap
		{
			GameObjectGUID get(GameObject gameobject) override { return { (u64)gameobject.index }; }
		} save_map;
		TextSerializer serializer(blob_out, save_map);
		serializeComponent(serializer, cmp.type, gameobject);
		
		InputBlob blob_in(blob_out);
		struct : ILoadGameObjectGUIDMap
		{
			GameObject get(GameObjectGUID guid) override { return { (int)guid.value }; }
		} load_map;
		TextDeserializer deserializer(blob_in, load_map);
		deserializeComponent(deserializer, clone, cmp.type, cmp.scene->getVersion());
	}
	return clone;
}


GameObject Project::createGameObject(const Vec3& position, const Quat& rotation)
{
	GameObjectData* data;
	GameObject gameobject;
	if (m_first_free_slot >= 0)
	{
		data = &m_entities[m_first_free_slot];
		gameobject.index = m_first_free_slot;
		if (data->next >= 0) m_entities[data->next].prev = -1;
		m_first_free_slot = data->next;
	}
	else
	{
		gameobject.index = m_entities.size();
		data = &m_entities.emplace();
	}
	data->position = position;
	data->rotation = rotation;
	data->scale = 1;
	data->name = -1;
	data->hierarchy = -1;
	data->components = 0;
	data->valid = true;
	m_gameobject_created.invoke(gameobject);

	return gameobject;
}


void Project::destroyGameObject(GameObject gameobject)
{
	if (!gameobject.isValid()) return;
	
	GameObjectData& gameobject_data = m_entities[gameobject.index];
	ASSERT(gameobject_data.valid);
	for (GameObject first_child = getFirstChild(gameobject); first_child.isValid(); first_child = getFirstChild(gameobject))
	{
		setParent(INVALID_GAMEOBJECT, first_child);
	}
	setParent(INVALID_GAMEOBJECT, gameobject);
	

	u64 mask = gameobject_data.components;
	for (int i = 0; i < ComponentType::MAX_TYPES_COUNT; ++i)
	{
		if ((mask & ((u64)1 << i)) != 0)
		{
			auto original_mask = mask;
			IScene* scene = m_component_type_map[i].scene;
			auto destroy_method = m_component_type_map[i].destroy;
			(scene->*destroy_method)(gameobject);
			mask = gameobject_data.components;
			ASSERT(original_mask != mask);
		}
	}

	gameobject_data.next = m_first_free_slot;
	gameobject_data.prev = -1;
	gameobject_data.hierarchy = -1;
	
	gameobject_data.valid = false;
	if (m_first_free_slot >= 0)
	{
		m_entities[m_first_free_slot].prev = gameobject.index;
	}

	if (gameobject_data.name >= 0)
	{
		m_entities[m_names.back().gameobject.index].name = gameobject_data.name;
		m_names.eraseFast(gameobject_data.name);
		gameobject_data.name = -1;
	}

	m_first_free_slot = gameobject.index;
	m_gameobject_destroyed.invoke(gameobject);
}


GameObject Project::getFirstGameObject() const
{
	for (int i = 0; i < m_entities.size(); ++i)
	{
		if (m_entities[i].valid) return {i};
	}
	return INVALID_GAMEOBJECT;
}


GameObject Project::getNextGameObject(GameObject gameobject) const
{
	for (int i = gameobject.index + 1; i < m_entities.size(); ++i)
	{
		if (m_entities[i].valid) return {i};
	}
	return INVALID_GAMEOBJECT;
}


GameObject Project::getParent(GameObject gameobject) const
{
	int idx = m_entities[gameobject.index].hierarchy;
	if (idx < 0) return INVALID_GAMEOBJECT;
	return m_hierarchy[idx].parent;
}


GameObject Project::getFirstChild(GameObject gameobject) const
{
	int idx = m_entities[gameobject.index].hierarchy;
	if (idx < 0) return INVALID_GAMEOBJECT;
	return m_hierarchy[idx].first_child;
}


GameObject Project::getNextSibling(GameObject gameobject) const
{
	int idx = m_entities[gameobject.index].hierarchy;
	if (idx < 0) return INVALID_GAMEOBJECT;
	return m_hierarchy[idx].next_sibling;
}


bool Project::isDescendant(GameObject ancestor, GameObject descendant) const
{
	if (!ancestor.isValid()) return false;
	for(GameObject e = getFirstChild(ancestor); e.isValid(); e = getNextSibling(e))
	{
		if (e == descendant) return true;
		if (isDescendant(e, descendant)) return true;
	}

	return false;
}


void Project::setParent(GameObject new_parent, GameObject child)
{
	bool would_create_cycle = isDescendant(child, new_parent);
	if (would_create_cycle)
	{
		g_log_error.log("Engine") << "Hierarchy can not contains a cycle.";
		return;
	}

	auto collectGarbage = [this](GameObject gameobject) {
		Hierarchy& h = m_hierarchy[m_entities[gameobject.index].hierarchy];
		if (h.parent.isValid()) return;
		if (h.first_child.isValid()) return;

		const Hierarchy& last = m_hierarchy.back();
		m_entities[last.gameobject.index].hierarchy = m_entities[gameobject.index].hierarchy;
		m_entities[gameobject.index].hierarchy = -1;
		h = last;
		m_hierarchy.pop();
	};

	int child_idx = m_entities[child.index].hierarchy;
	
	if (child_idx >= 0)
	{
		GameObject old_parent = m_hierarchy[child_idx].parent;

		if (old_parent.isValid())
		{
			Hierarchy& old_parent_h = m_hierarchy[m_entities[old_parent.index].hierarchy];
			GameObject* x = &old_parent_h.first_child;
			while (x->isValid())
			{
				if (*x == child)
				{
					*x = getNextSibling(child);
					break;
				}
				x = &m_hierarchy[m_entities[x->index].hierarchy].next_sibling;
			}
			m_hierarchy[child_idx].parent = INVALID_GAMEOBJECT;
			m_hierarchy[child_idx].next_sibling = INVALID_GAMEOBJECT;
			collectGarbage(old_parent);
			child_idx = m_entities[child.index].hierarchy;
		}
	}
	else if(new_parent.isValid())
	{
		child_idx = m_hierarchy.size();
		m_entities[child.index].hierarchy = child_idx;
		Hierarchy& h = m_hierarchy.emplace();
		h.gameobject = child;
		h.parent = INVALID_GAMEOBJECT;
		h.first_child = INVALID_GAMEOBJECT;
		h.next_sibling = INVALID_GAMEOBJECT;
	}

	if (new_parent.isValid())
	{
		int new_parent_idx = m_entities[new_parent.index].hierarchy;
		if (new_parent_idx < 0)
		{
			new_parent_idx = m_hierarchy.size();
			m_entities[new_parent.index].hierarchy = new_parent_idx;
			Hierarchy& h = m_hierarchy.emplace();
			h.gameobject = new_parent;
			h.parent = INVALID_GAMEOBJECT;
			h.first_child = INVALID_GAMEOBJECT;
			h.next_sibling = INVALID_GAMEOBJECT;
		}

		m_hierarchy[child_idx].parent = new_parent;
		Transform parent_tr = getTransform(new_parent);
		Transform child_tr = getTransform(child);
		m_hierarchy[child_idx].local_transform = parent_tr.inverted() * child_tr;
		m_hierarchy[child_idx].next_sibling = m_hierarchy[new_parent_idx].first_child;
		m_hierarchy[new_parent_idx].first_child = child;
	}
	else
	{
		if (child_idx >= 0) collectGarbage(child);
	}
}


void Project::updateGlobalTransform(GameObject gameobject)
{
	const Hierarchy& h = m_hierarchy[m_entities[gameobject.index].hierarchy];
	Transform parent_tr = getTransform(h.parent);
	
	Transform new_tr = parent_tr * h.local_transform;
	setTransform(gameobject, new_tr);
}


void Project::setLocalPosition(GameObject gameobject, const Vec3& pos)
{
	int hierarchy_idx = m_entities[gameobject.index].hierarchy;
	if (hierarchy_idx < 0)
	{
		setPosition(gameobject, pos);
		return;
	}

	m_hierarchy[hierarchy_idx].local_transform.pos = pos;
	updateGlobalTransform(gameobject);
}


void Project::setLocalRotation(GameObject gameobject, const Quat& rot)
{
	int hierarchy_idx = m_entities[gameobject.index].hierarchy;
	if (hierarchy_idx < 0)
	{
		setRotation(gameobject, rot);
		return;
	}
	m_hierarchy[hierarchy_idx].local_transform.rot = rot;
	updateGlobalTransform(gameobject);
}


Transform Project::computeLocalTransform(GameObject parent, const Transform& global_transform) const
{
	Transform parent_tr = getTransform(parent);
	return parent_tr.inverted() * global_transform;
}


void Project::setLocalTransform(GameObject gameobject, const Transform& transform)
{
	int hierarchy_idx = m_entities[gameobject.index].hierarchy;
	if (hierarchy_idx < 0)
	{
		setTransform(gameobject, transform);
		return;
	}

	Hierarchy& h = m_hierarchy[hierarchy_idx];
	h.local_transform = transform;
	updateGlobalTransform(gameobject);
}


Transform Project::getLocalTransform(GameObject gameobject) const
{
	int hierarchy_idx = m_entities[gameobject.index].hierarchy;
	if (hierarchy_idx < 0)
	{
		return getTransform(gameobject);
	}

	return m_hierarchy[hierarchy_idx].local_transform;
}


float Project::getLocalScale(GameObject gameobject) const
{
	int hierarchy_idx = m_entities[gameobject.index].hierarchy;
	if (hierarchy_idx < 0)
	{
		return getScale(gameobject);
	}

	return m_hierarchy[hierarchy_idx].local_transform.scale;
}



void Project::serializeComponent(ISerializer& serializer, ComponentType type, GameObject gameobject)
{
	auto* scene = m_component_type_map[type.index].scene;
	auto& method = m_component_type_map[type.index].serialize;
	(scene->*method)(serializer, gameobject);
}


void Project::deserializeComponent(IDeserializer& serializer, GameObject gameobject, ComponentType type, int scene_version)
{
	auto* scene = m_component_type_map[type.index].scene;
	auto& method = m_component_type_map[type.index].deserialize;
	(scene->*method)(serializer, gameobject, scene_version);
}


void Project::serialize(OutputBlob& serializer)
{
	serializer.write((i32)m_entities.size());
	serializer.write(&m_entities[0], sizeof(m_entities[0]) * m_entities.size());
	serializer.write((i32)m_names.size());
	for (const GameObjectName& name : m_names)
	{
		serializer.write(name.gameobject);
		serializer.writeString(name.name);
	}
	serializer.write(m_first_free_slot);

	serializer.write(m_hierarchy.size());
	if(!m_hierarchy.empty()) serializer.write(&m_hierarchy[0], sizeof(m_hierarchy[0]) * m_hierarchy.size());
}


void Project::deserialize(InputBlob& serializer)
{
	i32 count;
	serializer.read(count);
	m_entities.resize(count);

	if (count > 0) serializer.read(&m_entities[0], sizeof(m_entities[0]) * m_entities.size());

	serializer.read(count);
	for (int i = 0; i < count; ++i)
	{
		GameObjectName& name = m_names.emplace();
		serializer.read(name.gameobject);
		serializer.readString(name.name, lengthOf(name.name));
		m_entities[name.gameobject.index].name = m_names.size() - 1;
	}

	serializer.read(m_first_free_slot);

	serializer.read(count);
	m_hierarchy.resize(count);
	if (count > 0) serializer.read(&m_hierarchy[0], sizeof(m_hierarchy[0]) * m_hierarchy.size());
}


struct PrefabGameObjectGUIDMap : public ILoadGameObjectGUIDMap
{
	explicit PrefabGameObjectGUIDMap(const Array<GameObject>& _entities)
		: entities(_entities)
	{
	}


	GameObject get(GameObjectGUID guid) override
	{
		if (guid.value >= entities.size()) return INVALID_GAMEOBJECT;
		return entities[(int)guid.value];
	}


	const Array<GameObject>& entities;
};


GameObject Project::instantiatePrefab(const PrefabResource& prefab,
	const Vec3& pos,
	const Quat& rot,
	float scale)
{
	InputBlob blob(prefab.blob.getData(), prefab.blob.getPos());
	Array<GameObject> entities(m_allocator);
	PrefabGameObjectGUIDMap gameobject_map(entities);
	TextDeserializer deserializer(blob, gameobject_map);
	u32 version;
	deserializer.read(&version);
	if (version > (int)PrefabVersion::LAST)
	{
		g_log_error.log("Engine") << "Prefab " << prefab.getPath() << " has unsupported version.";
		return INVALID_GAMEOBJECT;
	}
	int count;
	deserializer.read(&count);
	int gameobject_idx = 0;
	entities.reserve(count);
	for (int i = 0; i < count; ++i)
	{
		entities.push(createGameObject({0, 0, 0}, {0, 0, 0, 1}));
	}
	while (blob.getPosition() < blob.getSize() && gameobject_idx < count)
	{
		u64 prefab;
		deserializer.read(&prefab);
		GameObject gameobject = entities[gameobject_idx];
		setTransform(gameobject, {pos, rot, scale});
		if (version > (int)PrefabVersion::WITH_HIERARCHY)
		{
			GameObject parent;

			deserializer.read(&parent);
			if (parent.isValid())
			{
				RigidTransform local_tr;
				deserializer.read(&local_tr);
				float scale;
				deserializer.read(&scale);
				setParent(parent, gameobject);
				setLocalTransform(gameobject, {local_tr.pos, local_tr.rot, scale});
			}
		}
		u32 cmp_type_hash;
		deserializer.read(&cmp_type_hash);
		while (cmp_type_hash != 0)
		{
			ComponentType cmp_type = Reflection::getComponentTypeFromHash(cmp_type_hash);
			int scene_version;
			deserializer.read(&scene_version);
			deserializeComponent(deserializer, gameobject, cmp_type, scene_version);
			deserializer.read(&cmp_type_hash);
		}
		++gameobject_idx;
	}
	return entities[0];
}


void Project::setScale(GameObject gameobject, float scale)
{
	auto& transform = m_entities[gameobject.index];
	transform.scale = scale;
	transformGameObject(gameobject, true);
}


float Project::getScale(GameObject gameobject) const
{
	auto& transform = m_entities[gameobject.index];
	return transform.scale;
}


ComponentUID Project::getFirstComponent(GameObject gameobject) const
{
	u64 mask = m_entities[gameobject.index].components;
	for (int i = 0; i < ComponentType::MAX_TYPES_COUNT; ++i)
	{
		if ((mask & (u64(1) << i)) != 0)
		{
			IScene* scene = m_component_type_map[i].scene;
			return ComponentUID(gameobject, {i}, scene);
		}
	}
	return ComponentUID::INVALID;
}


ComponentUID Project::getNextComponent(const ComponentUID& cmp) const
{
	u64 mask = m_entities[cmp.gameobject.index].components;
	for (int i = cmp.type.index + 1; i < ComponentType::MAX_TYPES_COUNT; ++i)
	{
		if ((mask & (u64(1) << i)) != 0)
		{
			IScene* scene = m_component_type_map[i].scene;
			return ComponentUID(cmp.gameobject, {i}, scene);
		}
	}
	return ComponentUID::INVALID;
}


ComponentUID Project::getComponent(GameObject gameobject, ComponentType component_type) const
{
	u64 mask = m_entities[gameobject.index].components;
	if ((mask & (u64(1) << component_type.index)) == 0) return ComponentUID::INVALID;
	IScene* scene = m_component_type_map[component_type.index].scene;
	return ComponentUID(gameobject, component_type, scene);
}


bool Project::hasComponent(GameObject gameobject, ComponentType component_type) const
{
	u64 mask = m_entities[gameobject.index].components;
	return (mask & (u64(1) << component_type.index)) != 0;
}


void Project::onComponentDestroyed(GameObject gameobject, ComponentType component_type, IScene* scene)
{
	auto mask = m_entities[gameobject.index].components;
	auto old_mask = mask;
	mask &= ~((u64)1 << component_type.index);
	ASSERT(old_mask != mask);
	m_entities[gameobject.index].components = mask;
	m_component_destroyed.invoke(ComponentUID(gameobject, component_type, scene));
}


void Project::createComponent(ComponentType type, GameObject gameobject)
{
	IScene* scene = m_component_type_map[type.index].scene;
	auto& create_method = m_component_type_map[type.index].create;
	(scene->*create_method)(gameobject);
}


void Project::destroyComponent(GameObject gameobject, ComponentType type)
{
	IScene* scene = m_component_type_map[type.index].scene;
	auto& destroy_method = m_component_type_map[type.index].destroy;
	(scene->*destroy_method)(gameobject);
}


void Project::onComponentCreated(GameObject gameobject, ComponentType component_type, IScene* scene)
{
	ComponentUID cmp(gameobject, component_type, scene);
	m_entities[gameobject.index].components |= (u64)1 << component_type.index;
	m_component_added.invoke(cmp);
}


} // namespace Malmy

#include "prefab_system.h"
#include "editor/asset_browser.h"
#include "editor/ieditor_command.h"
#include "editor/studio_app.h"
#include "editor/world_editor.h"
#include "engine/array.h"
#include "engine/blob.h"
#include "engine/crc32.h"
#include "engine/engine.h"
#include "engine/fs/os_file.h"
#include "engine/hash_map.h"
#include "engine/iplugin.h"
#include "engine/json_serializer.h"
#include "engine/log.h"
#include "engine/matrix.h"
#include "engine/prefab.h"
#include "engine/reflection.h"
#include "engine/resource.h"
#include "engine/resource_manager.h"
#include "engine/serializer.h"
#include "engine/string.h"
#include "engine/project/project.h"
#include "imgui/imgui.h"
#include <cstdlib>

namespace Malmy
{


class AssetBrowserPlugin MALMY_FINAL : public AssetBrowser::IPlugin
{
public:
	AssetBrowserPlugin(StudioApp& app, PrefabSystem& system)
		: system(system)
		, editor(editor)
	{
		app.getAssetBrowser().registerExtension("fab", PrefabResource::TYPE);
	}


	void onGUI(Resource* resource) override
	{
		if (ImGui::Button("instantiate"))
		{
			Array<GameObject> entities(editor.getAllocator());
			system.instantiatePrefab(*(PrefabResource*)resource, editor.getCameraRaycastHit(), {0, 0, 0, 1}, 1);
		}
	}


	void onResourceUnloaded(Resource* resource) override {}
	const char* getName() const override { return "Prefab"; }
	ResourceType getResourceType() const override { return PrefabResource::TYPE; }


	PrefabSystem& system;
	WorldEditor& editor;
};


class PrefabSystemImpl MALMY_FINAL : public PrefabSystem
{
	struct InstantiatePrefabCommand MALMY_FINAL : public IEditorCommand
	{
		InstantiatePrefabCommand(WorldEditor& editor)
			: editor(editor)
		{
		}


		~InstantiatePrefabCommand()
		{
			prefab->getResourceManager().unload(*prefab);
		}


		void createGameObjectGUIDRecursive(GameObject gameobject) const
		{
			if (!gameobject.isValid()) return;

			editor.createGameObjectGUID(gameobject);
			
			Project& project = *editor.getProject();
			createGameObjectGUIDRecursive(project.getFirstChild(gameobject));
			createGameObjectGUIDRecursive(project.getNextSibling(gameobject));
		}


		void destroyGameObjectRecursive(GameObject gameobject) const
		{
			if (!gameobject.isValid()) return;

			Project& project = *editor.getProject();
			destroyGameObjectRecursive(project.getFirstChild(gameobject));
			destroyGameObjectRecursive(project.getNextSibling(gameobject));

			project.destroyGameObject(gameobject);
			editor.destroyGameObjectGUID(gameobject);

		}


		bool execute() override
		{
			gameobject = INVALID_GAMEOBJECT;
			if (!prefab->isReady()) return false;
			auto& system = (PrefabSystemImpl&)editor.getPrefabSystem();

			gameobject = system.doInstantiatePrefab(*prefab, position, rotation, scale);
			editor.createGameObjectGUID(gameobject);
			createGameObjectGUIDRecursive(editor.getProject()->getFirstChild(gameobject));
			return true;
		}


		void undo() override
		{
			Project& project = *editor.getProject();

			destroyGameObjectRecursive(project.getFirstChild(gameobject));
			project.destroyGameObject(gameobject);
			editor.destroyGameObjectGUID(gameobject);
		}


		void serialize(JsonSerializer& serializer) override
		{
			serializer.serialize("position_x", position.x);
			serializer.serialize("position_y", position.y);
			serializer.serialize("position_z", position.z);
			serializer.serialize("rotation_x", rotation.x);
			serializer.serialize("rotation_y", rotation.y);
			serializer.serialize("rotation_z", rotation.z);
			serializer.serialize("rotation_w", rotation.w);
			serializer.serialize("scale", scale);
			serializer.serialize("path", prefab->getPath().c_str());
		}


		void deserialize(JsonDeserializer& serializer) override
		{
			serializer.deserialize("position_x", position.x, 0);
			serializer.deserialize("position_y", position.y, 0);
			serializer.deserialize("position_z", position.z, 0);
			serializer.deserialize("rotation_x", rotation.x, 0);
			serializer.deserialize("rotation_y", rotation.y, 0);
			serializer.deserialize("rotation_z", rotation.z, 0);
			serializer.deserialize("rotation_w", rotation.w, 0);
			serializer.deserialize("scale", scale, 0);
			char path[MAX_PATH_LENGTH];
			serializer.deserialize("path_hash", path, lengthOf(path), "");
			prefab = (PrefabResource*)editor.getEngine().getResourceManager().get(PrefabResource::TYPE)->load(Path(path));
		}


		const char* getType() override
		{
			return "instantiate_prefab";
		}


		bool merge(IEditorCommand& command) override { return false; }

		PrefabResource* prefab;
		Vec3 position;
		Quat rotation;
		float scale;
		WorldEditor& editor;
		GameObject gameobject;
	};

public:
	explicit PrefabSystemImpl(WorldEditor& editor)
		: m_editor(editor)
		, m_project(nullptr)
		, m_instances(editor.getAllocator())
		, m_resources(editor.getAllocator())
		, m_prefabs(editor.getAllocator())
	{
		editor.projectCreated().bind<PrefabSystemImpl, &PrefabSystemImpl::onProjectCreated>(this);
		editor.projectDestroyed().bind<PrefabSystemImpl, &PrefabSystemImpl::onProjectDestroyed>(this);
		setProject(editor.getProject());
		editor.registerEditorCommandCreator(
			"instantiate_prefab", &PrefabSystemImpl::createInstantiatePrefabCommand);
	}


	~PrefabSystemImpl()
	{
		m_editor.projectCreated()
			.unbind<PrefabSystemImpl, &PrefabSystemImpl::onProjectCreated>(this);
		m_editor.projectDestroyed()
			.unbind<PrefabSystemImpl, &PrefabSystemImpl::onProjectDestroyed>(this);
		setProject(nullptr);
	}


	static IEditorCommand* createInstantiatePrefabCommand(WorldEditor& editor)
	{
		return MALMY_NEW(editor.getAllocator(), InstantiatePrefabCommand)(editor);
	}


	WorldEditor& getEditor() const { return m_editor; }


	void setProject(Project* project)
	{
		if (m_project)
		{
			m_project->gameobjectDestroyed()
				.unbind<PrefabSystemImpl, &PrefabSystemImpl::onGameObjectDestroyed>(
					this);
		}
		m_project = project;
		if (m_project)
		{
			m_project->gameobjectDestroyed()
				.bind<PrefabSystemImpl, &PrefabSystemImpl::onGameObjectDestroyed>(this);
		}
	}


	void onProjectCreated()
	{
		m_instances.clear();
		for (PrefabResource* prefab : m_resources)
		{
			prefab->getResourceManager().unload(*prefab);
		}
		m_resources.clear();
		m_prefabs.clear();
		setProject(m_editor.getProject());
	}


	void onProjectDestroyed()
	{
		m_instances.clear();
		for (PrefabResource* prefab : m_resources)
		{
			prefab->getResourceManager().unload(*prefab);
		}
		m_resources.clear();
		m_prefabs.clear();
		setProject(nullptr);
	}


	void link(GameObject gameobject, u64 prefab)
	{
		ASSERT(prefab != 0);
		int idx = m_instances.find(prefab);
		m_prefabs[gameobject.index].prev = INVALID_GAMEOBJECT;
		if (idx >= 0)
		{
			GameObject e = m_instances.at(idx);
			m_prefabs[e.index].prev = gameobject;
			m_prefabs[gameobject.index].next = e;
		}
		else
		{
			m_prefabs[gameobject.index].next = INVALID_GAMEOBJECT;
		}
		m_instances[prefab] = gameobject;
	}


	void unlink(GameObject gameobject)
	{
		GameObjectPrefab& p = m_prefabs[gameobject.index];
		if (p.prefab == 0) return;
		if (m_instances[p.prefab] == gameobject)
		{
			if (m_prefabs[gameobject.index].next.isValid())
				m_instances[p.prefab] = m_prefabs[gameobject.index].next;
			else
				m_instances.erase(p.prefab);
		}
		if (p.prev.isValid()) m_prefabs[p.prev.index].next = p.next;
		if (p.next.isValid()) m_prefabs[p.next.index].prev = p.prev;
	}


	void onGameObjectDestroyed(GameObject gameobject)
	{
		if (gameobject.index >= m_prefabs.size()) return;
		unlink(gameobject);
		m_prefabs[gameobject.index].prefab = 0;
	}


	void setPrefab(GameObject gameobject, u64 prefab) override
	{
		reserve(gameobject);
		m_prefabs[gameobject.index].prefab = prefab;
		link(gameobject, prefab);
	}


	PrefabResource* getPrefabResource(GameObject gameobject) override
	{
		if (gameobject.index >= m_prefabs.size()) return nullptr;
		u32 hash = u32(m_prefabs[gameobject.index].prefab & 0xffffFFFF);
		auto iter = m_resources.find(hash);
		if (!iter.isValid()) return nullptr;
		return iter.value();
	}


	u64 getPrefab(GameObject gameobject) const override
	{
		if (gameobject.index >= m_prefabs.size()) return 0;
		return m_prefabs[gameobject.index].prefab;
	}


	int getMaxGameObjectIndex() const override
	{
		return m_prefabs.size();
	}


	GameObject getFirstInstance(u64 prefab) override
	{
		int instances_index = m_instances.find(prefab);
		if (instances_index >= 0) return m_instances.at(instances_index);
		return INVALID_GAMEOBJECT;
	}


	GameObject getNextInstance(GameObject gameobject) override
	{
		return m_prefabs[gameobject.index].next;
	}


	void reserve(GameObject gameobject)
	{
		while (gameobject.index >= m_prefabs.size())
		{
			auto& i = m_prefabs.emplace();
			i.prefab = 0;
		}
	}


	struct LoadGameObjectGUIDMap : public ILoadGameObjectGUIDMap
	{
		explicit LoadGameObjectGUIDMap(const Array<GameObject>& entities)
			: entities(entities)
		{
		}


		GameObject get(GameObjectGUID guid) override
		{
			if (guid.value >= entities.size()) return INVALID_GAMEOBJECT;
			return entities[(int)guid.value];
		}


		const Array<GameObject>& entities;
	};


	struct SaveGameObjectGUIDMap : public ISaveGameObjectGUIDMap
	{
		explicit SaveGameObjectGUIDMap(const Array<GameObject>& entities)
			: entities(entities)
		{
		}


		GameObjectGUID get(GameObject gameobject) override
		{
			int idx = entities.indexOf(gameobject);
			if (idx < 0) return INVALID_GAMEOBJECT_GUID;
			return {(u64)idx};
		}


		const Array<GameObject>& entities;
	};


	GameObject doInstantiatePrefab(PrefabResource& prefab_res, const Vec3& pos, const Quat& rot, float scale)
	{
		if (!prefab_res.isReady()) return INVALID_GAMEOBJECT;
		if (!m_resources.find(prefab_res.getPath().getHash()).isValid())
		{
			m_resources.insert(prefab_res.getPath().getHash(), &prefab_res);
			prefab_res.getResourceManager().load(prefab_res);
		}
		InputBlob blob(prefab_res.blob.getData(), prefab_res.blob.getPos());
		Array<GameObject> entities(m_editor.getAllocator());
		LoadGameObjectGUIDMap gameobject_map(entities);
		TextDeserializer deserializer(blob, gameobject_map);
		u32 version;
		deserializer.read(&version);
		if (version > (int)PrefabVersion::LAST)
		{
			g_log_error.log("Editor") << "Prefab " << prefab_res.getPath() << " has unsupported version.";
			return INVALID_GAMEOBJECT;
		}
		int count;
		deserializer.read(&count);
		entities.reserve(count);
		for (int i = 0; i < count; ++i)
		{
			entities.push(m_project->createGameObject({0, 0, 0}, {0, 0, 0, 1}));
		}

		int gameobject_idx = 0;
		while (blob.getPosition() < blob.getSize() && gameobject_idx < count)
		{
			u64 prefab;
			deserializer.read(&prefab);
			GameObject gameobject = entities[gameobject_idx];
			m_project->setTransform(gameobject, {pos, rot, scale});
			reserve(gameobject);
			m_prefabs[gameobject.index].prefab = prefab;
			link(gameobject, prefab);
			
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
					m_project->setParent(parent, gameobject);
					m_project->setLocalTransform(gameobject, {local_tr.pos, local_tr.rot, scale});
				}
			}
			u32 cmp_type_hash;
			deserializer.read(&cmp_type_hash);
			while (cmp_type_hash != 0)
			{
				ComponentType cmp_type = Reflection::getComponentTypeFromHash(cmp_type_hash);
				int scene_version;
				deserializer.read(&scene_version);
				m_project->deserializeComponent(deserializer, gameobject, cmp_type, scene_version);
				deserializer.read(&cmp_type_hash);
			}
			++gameobject_idx;
		}
		return entities[0];
	}


	GameObject instantiatePrefab(PrefabResource& prefab, const Vec3& pos, const Quat& rot, float scale) override
	{
		InstantiatePrefabCommand* cmd = MALMY_NEW(m_editor.getAllocator(), InstantiatePrefabCommand)(m_editor);
		cmd->position = pos;
		prefab.getResourceManager().load(prefab);
		cmd->prefab = &prefab;
		cmd->rotation = rot;
		cmd->scale = scale;
		m_editor.executeCommand(cmd);
		return cmd ? cmd->gameobject : INVALID_GAMEOBJECT;
	}


	static int countHierarchy(Project* project, GameObject gameobject)
	{
		if (!gameobject.isValid()) return 0;
		int children_count = countHierarchy(project, project->getFirstChild(gameobject));
		int siblings_count = countHierarchy(project, project->getNextSibling(gameobject));
		return 1 + children_count + siblings_count;
	}


	static void serializePrefabGameObject(u64 prefab,
		int& index,
		TextSerializer& serializer,
		Project* project,
		GameObject gameobject,
		bool is_root)
	{
		if (!gameobject.isValid()) return;

		prefab |= ((u64)index) << 32;
		++index;
		serializer.write("prefab", prefab);
		GameObject parent = is_root ? INVALID_GAMEOBJECT : project->getParent(gameobject);
		serializer.write("parent", parent);
		if (parent.isValid())
		{
			serializer.write("local_transform", project->getLocalTransform(gameobject).getRigidPart());
			serializer.write("local_scale", project->getLocalScale(gameobject));
		}
		for (ComponentUID cmp = project->getFirstComponent(gameobject); cmp.isValid();
			cmp = project->getNextComponent(cmp))
		{
			const char* cmp_name = Reflection::getComponentTypeID(cmp.type.index);
			u32 type_hash = Reflection::getComponentTypeHash(cmp.type);
			serializer.write(cmp_name, type_hash);
			int scene_version = project->getScene(cmp.type)->getVersion();
			serializer.write("scene_version", scene_version);
			project->serializeComponent(serializer, cmp.type, cmp.gameobject);
		}
		serializer.write("cmp_end", 0);

		serializePrefabGameObject(prefab, index, serializer, project, project->getFirstChild(gameobject), false);
		if (!is_root)
		{
			serializePrefabGameObject(prefab, index, serializer, project, project->getNextSibling(gameobject), false);
		}
	}



	static void serializePrefab(Project* project,
		GameObject root,
		const Path& path,
		TextSerializer& serializer)
	{
		serializer.write("version", (u32)PrefabVersion::LAST);
		int count = 1 + countHierarchy(project, project->getFirstChild(root));
		serializer.write("gameobject_count", count);
		int i = 0;
		u64 prefab = path.getHash();
		serializePrefabGameObject(prefab, i, serializer, project, root, true);
	}


	GameObject getPrefabRoot(GameObject gameobject) const
	{
		GameObject root = gameobject;
		GameObject parent = m_project->getParent(root);
		while (parent.isValid() && getPrefab(parent) != 0)
		{
			root = parent;
			parent = m_project->getParent(root);
		}
		return root;
	}


	void gatherHierarchy(GameObject gameobject, bool is_root, Array<GameObject>& out) const
	{
		if (!gameobject.isValid()) return;

		out.push(gameobject);
		gatherHierarchy(m_project->getFirstChild(gameobject), false, out);
		gatherHierarchy(m_project->getNextSibling(gameobject), false, out);
	}


	void savePrefab(const Path& path) override
	{
		auto& selected_entities = m_editor.getSelectedEntities();
		if (selected_entities.size() != 1) return;

		GameObject gameobject = selected_entities[0];
		u64 prefab = getPrefab(gameobject);
		if (prefab != 0) gameobject = getPrefabRoot(gameobject);

		FS::OsFile file;
		if (!file.open(path.c_str(), FS::Mode::CREATE_AND_WRITE))
		{
			g_log_error.log("Editor") << "Failed to create " << path.c_str();
			return;
		}

		Array<GameObject> entities(m_editor.getAllocator());
		gatherHierarchy(gameobject, true, entities);
		OutputBlob blob(m_editor.getAllocator());
		SaveGameObjectGUIDMap gameobject_map(entities);
		TextSerializer serializer(blob, gameobject_map);

		serializePrefab(m_project, entities[0], path, serializer);

		file.write(blob.getData(), blob.getPos());

		file.close();

		if (prefab == 0)
		{
			m_editor.beginCommandGroup(crc32("save_prefab"));

			Transform tr = m_project->getTransform(gameobject);
			m_editor.destroyEntities(&entities[0], entities.size());
			auto* resource_manager = m_editor.getEngine().getResourceManager().get(PrefabResource::TYPE);
			auto* res = (PrefabResource*)resource_manager->load(path);
			FS::FileSystem& fs = m_editor.getEngine().getFileSystem();
			while (fs.hasWork()) fs.updateAsyncTransactions();
			instantiatePrefab(*res, tr.pos, tr.rot, tr.scale);

			m_editor.endCommandGroup();
		}
	}


	void serialize(OutputBlob& serializer) override
	{
		serializer.write(m_prefabs.size());
		if(!m_prefabs.empty()) serializer.write(&m_prefabs[0], m_prefabs.size() * sizeof(m_prefabs[0]));
		serializer.write(m_instances.size());
		for (int i = 0, c = m_instances.size(); i < c; ++i)
		{
			serializer.write(m_instances.getKey(i));
			serializer.write(m_instances.at(i));
		}
		serializer.write(m_resources.size());
		for (PrefabResource* res : m_resources)
		{
			serializer.writeString(res->getPath().c_str());
		}
	}


	void deserialize(InputBlob& serializer) override
	{
		int count;
		serializer.read(count);
		m_prefabs.resize(count);
		if (count > 0)
			serializer.read(&m_prefabs[0], m_prefabs.size() * sizeof(m_prefabs[0]));
		serializer.read(count);
		for (int i = 0; i < count; ++i)
		{
			u64 key;
			GameObject value;
			serializer.read(key);
			serializer.read(value);
			m_instances.insert(key, value);
		}
		serializer.read(count);
		auto* resource_manager = m_editor.getEngine().getResourceManager().get(PrefabResource::TYPE);
		for (int i = 0; i < count; ++i)
		{
			char tmp[MAX_PATH_LENGTH];
			serializer.readString(tmp, lengthOf(tmp));
			auto* res = (PrefabResource*)resource_manager->load(Path(tmp));
			m_resources.insert(res->getPath().getHash(), res);
		}
	}

	
	void serialize(ISerializer& serializer) override
	{
		serializer.write("count", m_prefabs.size());

		for (PrefabResource* res : m_resources)
		{
			serializer.write("resource", res->getPath().c_str());
		}
		serializer.write("resource", "");

		for (int i = 0; i < m_instances.size(); ++i)
		{
			u64 prefab = m_instances.getKey(i);
			if ((prefab & 0xffffFFFF) != prefab) continue;
			GameObject gameobject = m_instances.at(i);
			while(gameobject.isValid())
			{
				serializer.write("prefab", (u32)prefab);
				serializer.write("pos", m_project->getPosition(gameobject));
				serializer.write("rot", m_project->getRotation(gameobject));
				serializer.write("scale", m_project->getScale(gameobject));
				gameobject = m_prefabs[gameobject.index].next;
			}
		}
		serializer.write("prefab", (u32)0);
	}


	void deserialize(IDeserializer& serializer) override
	{
		int count;
		serializer.read(&count);
		reserve({count-1});
		
		auto* mng = m_editor.getEngine().getResourceManager().get(PrefabResource::TYPE);
		for (;;)
		{
			char tmp[MAX_PATH_LENGTH];
			serializer.read(tmp, lengthOf(tmp));
			if (tmp[0] == 0) break;
			auto* res = (PrefabResource*)mng->load(Path(tmp));
			m_resources.insert(res->getPath().getHash(), res);
		}

		while(m_editor.getEngine().getFileSystem().hasWork())
			m_editor.getEngine().getFileSystem().updateAsyncTransactions();

		for (;;)
		{
			u32 res_hash;
			serializer.read(&res_hash);
			if (res_hash == 0) break;
			
			Vec3 pos;
			serializer.read(&pos);
			Quat rot;
			serializer.read(&rot);
			float scale;
			serializer.read(&scale);
			doInstantiatePrefab(*m_resources[res_hash], pos, rot, scale);
		}
	}


private:
	struct GameObjectPrefab
	{
		u64 prefab;
		GameObject next;
		GameObject prev;
	};
	Array<GameObjectPrefab> m_prefabs;
	AssociativeArray<u64, GameObject> m_instances;
	HashMap<u32, PrefabResource*> m_resources;
	Project* m_project;
	WorldEditor& m_editor;
	StudioApp* m_app;
}; // class PrefabSystemImpl


PrefabSystem* PrefabSystem::create(WorldEditor& editor)
{
	return MALMY_NEW(editor.getAllocator(), PrefabSystemImpl)(editor);
}


void PrefabSystem::destroy(PrefabSystem* system)
{
	MALMY_DELETE(
		static_cast<PrefabSystemImpl*>(system)->getEditor().getAllocator(), system);
}


static AssetBrowserPlugin* ab_plugin = nullptr;


void PrefabSystem::createAssetBrowserPlugin(StudioApp& app, PrefabSystem& system)
{
	ab_plugin = MALMY_NEW(app.getWorldEditor().getAllocator(), AssetBrowserPlugin)(app, system);
	app.getAssetBrowser().addPlugin(*ab_plugin);
}


void PrefabSystem::destroyAssetBrowserPlugin(StudioApp& app)
{
	app.getAssetBrowser().removePlugin(*ab_plugin);
	MALMY_DELETE(app.getWorldEditor().getAllocator(), ab_plugin);
	ab_plugin = nullptr;
}


} // namespace Malmy
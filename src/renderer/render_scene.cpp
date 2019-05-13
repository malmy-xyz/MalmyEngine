#include "render_scene.h"

#include "engine/array.h"
#include "engine/blob.h"
#include "engine/crc32.h"
#include "engine/engine.h"
#include "engine/fs/file_system.h"
#include "engine/geometry.h"
#include "engine/job_system.h"
#include "engine/log.h"
#include "engine/lua_wrapper.h"
#include "engine/math_utils.h"
#include "engine/plugin_manager.h"
#include "engine/profiler.h"
#include "engine/reflection.h"
#include "engine/resource_manager.h"
#include "engine/resource_manager_base.h"
#include "engine/serializer.h"
#include "engine/project/project.h"
#include "lua_script/lua_script_system.h"
#include "renderer/culling_system.h"
#include "renderer/font_manager.h"
#include "renderer/frame_buffer.h"
#include "renderer/material.h"
#include "renderer/material_manager.h"
#include "renderer/model.h"
#include "renderer/particle_system.h"
#include "renderer/pipeline.h"
#include "renderer/pose.h"
#include "renderer/renderer.h"
#include "renderer/terrain.h"
#include "renderer/texture.h"
#include <cfloat>
#include <cmath>
#include <algorithm>


namespace Malmy
{


enum class RenderSceneVersion : int
{
	GRASS_ROTATION_MODE,
	GLOBAL_LIGHT_REFACTOR,
	EMITTER_MATERIAL,
	BONE_ATTACHMENT_TRANSFORM,
	MODEL_INSTNACE_FLAGS,
	INDIRECT_INTENSITY,
	SCRIPTED_PARTICLES,
	POINT_LIGHT_NO_COMPONENT,
	MODEL_INSTANCE_ENABLE,
	ENVIRONMENT_PROBE_FLAGS,

	LATEST
};


static const ComponentType MODEL_INSTANCE_TYPE = Reflection::getComponentType("renderable");
static const ComponentType DECAL_TYPE = Reflection::getComponentType("decal");
static const ComponentType POINT_LIGHT_TYPE = Reflection::getComponentType("point_light");
static const ComponentType PARTICLE_EMITTER_TYPE = Reflection::getComponentType("particle_emitter");
static const ComponentType SCRIPTED_PARTICLE_EMITTER_TYPE = Reflection::getComponentType("scripted_particle_emitter");
static const ComponentType PARTICLE_EMITTER_ALPHA_TYPE = Reflection::getComponentType("particle_emitter_alpha");
static const ComponentType PARTICLE_EMITTER_FORCE_HASH = Reflection::getComponentType("particle_emitter_force");
static const ComponentType PARTICLE_EMITTER_ATTRACTOR_TYPE =
	Reflection::getComponentType("particle_emitter_attractor");
static const ComponentType PARTICLE_EMITTER_SUBIMAGE_TYPE =
	Reflection::getComponentType("particle_emitter_subimage");
static const ComponentType PARTICLE_EMITTER_LINEAR_MOVEMENT_TYPE =
	Reflection::getComponentType("particle_emitter_linear_movement");
static const ComponentType PARTICLE_EMITTER_SPAWN_SHAPE_TYPE =
	Reflection::getComponentType("particle_emitter_spawn_shape");
static const ComponentType PARTICLE_EMITTER_PLANE_TYPE = Reflection::getComponentType("particle_emitter_plane");
static const ComponentType PARTICLE_EMITTER_RANDOM_ROTATION_TYPE =
	Reflection::getComponentType("particle_emitter_random_rotation");
static const ComponentType PARTICLE_EMITTER_SIZE_TYPE = Reflection::getComponentType("particle_emitter_size");
static const ComponentType GLOBAL_LIGHT_TYPE = Reflection::getComponentType("global_light");
static const ComponentType CAMERA_TYPE = Reflection::getComponentType("camera");
static const ComponentType TERRAIN_TYPE = Reflection::getComponentType("terrain");
static const ComponentType BONE_ATTACHMENT_TYPE = Reflection::getComponentType("bone_attachment");
static const ComponentType ENVIRONMENT_PROBE_TYPE = Reflection::getComponentType("environment_probe");
static const ComponentType TEXT_MESH_TYPE = Reflection::getComponentType("text_mesh");

template <int Size>
struct LambdaStorage
{
	template <typename T>
	static void call(void* f)
	{
		(*(T*)f)();
	}

	template <typename T> 
	void operator=(const T&& lambda) {
		static_assert(sizeof(lambda) <= sizeof(data), "not enough storage");
		new (data) T(lambda);
		caller = &call<T>;
	}
	
	void invoke() {
		caller(data);
	}

	u8 data[Size];
	void (*caller)(void*);
};


struct Decal : public DecalInfo
{
	GameObject gameobject;
	Vec3 scale;
};


struct PointLight
{
	Vec3 m_diffuse_color;
	Vec3 m_specular_color;
	float m_diffuse_intensity;
	float m_specular_intensity;
	GameObject m_gameobject;
	float m_fov;
	float m_attenuation_param;
	float m_range;
	bool m_cast_shadows;
};


struct GlobalLight
{
	Vec3 m_diffuse_color;
	float m_diffuse_intensity;
	float m_indirect_intensity;
	Vec3 m_fog_color;
	float m_fog_density;
	float m_fog_bottom;
	float m_fog_height;
	GameObject m_gameobject;
	Vec4 m_cascades;
};


struct Camera
{
	static const int MAX_SLOT_LENGTH = 30;

	GameObject gameobject;
	float fov;
	float aspect;
	float near;
	float far;
	float ortho_size;
	float screen_width;
	float screen_height;
	bool is_ortho;
	char slot[MAX_SLOT_LENGTH + 1];
};


struct EnvironmentProbe
{
	enum Flags
	{
		REFLECTION = 1 << 0,
		OVERRIDE_GLOBAL_SIZE = 1 << 1
	};

	Texture* texture;
	Texture* irradiance;
	Texture* radiance;
	u64 guid;
	FlagSet<Flags, u32> flags;
	u16 radiance_size = 128;
	u16 irradiance_size = 32;
	u16 reflection_size = 1024;
};


struct BoneAttachment
{
	GameObject gameobject;
	GameObject parent_gameobject;
	int bone_index;
	RigidTransform relative_transform;
};


struct TextMesh
{
	enum Flags : u32
	{
		CAMERA_ORIENTED = 1 << 0
	};
	
	TextMesh(IAllocator& allocator) : text("", allocator) {}
	~TextMesh() { setFontResource(nullptr); }

	void setFontResource(FontResource* res)
	{
		if (m_font_resource)
		{
			if (m_font)
			{
				m_font_resource->removeRef(*m_font);
				m_font = nullptr;
			}
			m_font_resource->getObserverCb().unbind<TextMesh, &TextMesh::onFontLoaded>(this);
			m_font_resource->getResourceManager().unload(*m_font_resource);
		}
		m_font_resource = res;
		if (res) res->onLoaded<TextMesh, &TextMesh::onFontLoaded>(this); 
	}

	void onFontLoaded(Resource::State, Resource::State new_state, Resource&)
	{
		if (new_state != Resource::State::READY)
		{
			m_font = nullptr;
		}
		else
		{
			m_font = m_font_resource->addRef(m_font_size);
		}
	}

	void setFontSize(int value)
	{
		m_font_size = value;
		if (m_font_resource && m_font_resource->isReady())
		{
			if(m_font) m_font_resource->removeRef(*m_font);
			m_font = m_font_resource->addRef(m_font_size);
		}
	}

	FontResource* getFontResource() const { return m_font_resource; }
	Font* getFont() const { return m_font; }
	int getFontSize() const { return m_font_size; }

	string text;
	u32 color = 0xff000000;
	FlagSet<Flags, u32> m_flags;

private:
	int m_font_size = 13;
	Font* m_font = nullptr;
	FontResource* m_font_resource = nullptr;

};


class RenderSceneImpl MALMY_FINAL : public RenderScene
{
private:
	struct ModelLoadedCallback
	{
		ModelLoadedCallback(RenderSceneImpl& scene, Model* model)
			: m_scene(scene)
			, m_ref_count(0)
			, m_model(model)
		{
			m_model->getObserverCb().bind<RenderSceneImpl, &RenderSceneImpl::modelStateChanged>(&scene);
		}

		~ModelLoadedCallback()
		{
			m_model->getObserverCb().unbind<RenderSceneImpl, &RenderSceneImpl::modelStateChanged>(&m_scene);
		}

		Model* m_model;
		int m_ref_count;
		RenderSceneImpl& m_scene;
	};

public:
	RenderSceneImpl(Renderer& renderer,
		Engine& engine,
		Project& project,
		IAllocator& allocator);

	~RenderSceneImpl()
	{
		m_project.gameobjectTransformed().unbind<RenderSceneImpl, &RenderSceneImpl::onGameObjectMoved>(this);
		m_project.gameobjectDestroyed().unbind<RenderSceneImpl, &RenderSceneImpl::onGameObjectDestroyed>(this);
		CullingSystem::destroy(*m_culling_system);
	}


	void modelStateChanged(Resource::State old_state, Resource::State new_state, Resource& resource)
	{
		Model* model = static_cast<Model*>(&resource);
		if (new_state == Resource::State::READY)
		{
			modelLoaded(model);
		}
		else if (old_state == Resource::State::READY && new_state != Resource::State::READY)
		{
			modelUnloaded(model);
		}
	}


	void clear() override
	{
		auto& rm = m_engine.getResourceManager();
		auto* material_manager = static_cast<MaterialManager*>(rm.get(Material::TYPE));

		m_model_loaded_callbacks.clear();
		
		for (TextMesh* text_mesh : m_text_meshes)
		{
			MALMY_DELETE(m_allocator, text_mesh);
		}
		m_text_meshes.clear();

		for (Decal& decal : m_decals)
		{
			if (decal.material) material_manager->unload(*decal.material);
		}
		m_decals.clear();

		m_cameras.clear();

		for (auto* terrain : m_terrains)
		{
			MALMY_DELETE(m_allocator, terrain);
		}
		m_terrains.clear();

		for (auto* emitter : m_particle_emitters)
		{
			MALMY_DELETE(m_allocator, emitter);
		}
		m_particle_emitters.clear();
		for (auto* emitter : m_scripted_particle_emitters)
		{
			MALMY_DELETE(m_allocator, emitter);
		}
		m_scripted_particle_emitters.clear();

		for (auto& i : m_model_instances)
		{
			if (i.gameobject != INVALID_GAMEOBJECT && i.model)
			{
				freeCustomMeshes(i, material_manager);
				i.model->getResourceManager().unload(*i.model);
				MALMY_DELETE(m_allocator, i.pose);
			}
		}
		m_model_instances.clear();
		m_culling_system->clear();

		for (auto& probe : m_environment_probes)
		{
			if (probe.texture) probe.texture->getResourceManager().unload(*probe.texture);
			if (probe.radiance) probe.radiance->getResourceManager().unload(*probe.radiance);
			if (probe.irradiance) probe.irradiance->getResourceManager().unload(*probe.irradiance);
		}
		m_environment_probes.clear();
	}


	void resetParticleEmitter(GameObject gameobject) override
	{
		m_particle_emitters[gameobject]->reset();
	}


	ParticleEmitter* getParticleEmitter(GameObject gameobject) override
	{
		return m_particle_emitters[gameobject];
	}


	void updateEmitter(GameObject gameobject, float time_delta) override
	{
		m_particle_emitters[gameobject]->update(time_delta);
	}


	Project& getProject() override { return m_project; }


	IPlugin& getPlugin() const override { return m_renderer; }


	Int2 getParticleEmitterSpawnCount(GameObject gameobject) override
	{
		Int2 ret;
		ParticleEmitter* emitter = m_particle_emitters[gameobject];
		ret.x = emitter->m_spawn_count.from;
		ret.y = emitter->m_spawn_count.to;
		return ret;
	}


	void setParticleEmitterSpawnCount(GameObject gameobject, const Int2& value) override
	{
		ParticleEmitter* emitter = m_particle_emitters[gameobject];
		emitter->m_spawn_count.from = value.x;
		emitter->m_spawn_count.to = Math::maximum(value.x, value.y);
	}



	void getRay(GameObject camera_gameobject,
		const Vec2& screen_pos,
		Vec3& origin,
		Vec3& dir) override
	{
		Camera& camera = m_cameras[camera_gameobject];
		origin = m_project.getPosition(camera_gameobject);

		float width = camera.screen_width;
		float height = camera.screen_height;
		if (width <= 0 || height <= 0)
		{
			dir = m_project.getRotation(camera_gameobject).rotate(Vec3(0, 0, 1));
			return;
		}

		float nx = 2 * (screen_pos.x / width) - 1;
		float ny = 2 * ((height - screen_pos.y) / height) - 1;

		Matrix projection_matrix = getCameraProjection(camera_gameobject);
		Matrix view_matrix = m_project.getMatrix(camera_gameobject);

		if (camera.is_ortho)
		{
			float ratio = camera.screen_height > 0 ? camera.screen_width / camera.screen_height : 1;
			origin += view_matrix.getXVector() * nx * camera.ortho_size * ratio
				+ view_matrix.getYVector() * ny * camera.ortho_size;
		}

		view_matrix.inverse();
		Matrix inverted = (projection_matrix * view_matrix);
		inverted.inverse();

		Vec4 p0 = inverted * Vec4(nx, ny, -1, 1);
		Vec4 p1 = inverted * Vec4(nx, ny, 1, 1);
		p0 *= 1 / p0.w;
		p1 *= 1 / p1.w;
		dir = (p1 - p0).xyz();
		dir.normalize();
	}


	Frustum getCameraFrustum(GameObject gameobject) const override
	{
		const Camera& camera = m_cameras[gameobject];
		Matrix mtx = m_project.getMatrix(gameobject);
		Frustum ret;
		float ratio = camera.screen_height > 0 ? camera.screen_width / camera.screen_height : 1;
		if (camera.is_ortho)
		{
			ret.computeOrtho(mtx.getTranslation(),
				mtx.getZVector(),
				mtx.getYVector(),
				camera.ortho_size * ratio,
				camera.ortho_size,
				camera.near,
				camera.far);
			return ret;
		}
		ret.computePerspective(mtx.getTranslation(),
			-mtx.getZVector(),
			mtx.getYVector(),
			camera.fov,
			ratio,
			camera.near,
			camera.far);

		return ret;
	}


	Frustum getCameraFrustum(GameObject gameobject, const Vec2& viewport_min_px, const Vec2& viewport_max_px) const override
	{
		const Camera& camera = m_cameras[gameobject];
		Matrix mtx = m_project.getMatrix(gameobject);
		Frustum ret;
		float ratio = camera.screen_height > 0 ? camera.screen_width / camera.screen_height : 1;
		Vec2 viewport_min = { viewport_min_px.x / camera.screen_width * 2 - 1, (1 - viewport_max_px.y / camera.screen_height) * 2 - 1 };
		Vec2 viewport_max = { viewport_max_px.x / camera.screen_width * 2 - 1, (1 - viewport_min_px.y / camera.screen_height) * 2 - 1 };
		if (camera.is_ortho)
		{
			ret.computeOrtho(mtx.getTranslation(),
				mtx.getZVector(),
				mtx.getYVector(),
				camera.ortho_size * ratio,
				camera.ortho_size,
				camera.near,
				camera.far,
				viewport_min,
				viewport_max);
			return ret;
		}
		ret.computePerspective(mtx.getTranslation(),
			-mtx.getZVector(),
			mtx.getYVector(),
			camera.fov,
			ratio,
			camera.near,
			camera.far,
			viewport_min,
			viewport_max);

		return ret;
	}


	void updateBoneAttachment(const BoneAttachment& bone_attachment)
	{
		if (!bone_attachment.parent_gameobject.isValid()) return;
		GameObject model_instance = bone_attachment.parent_gameobject;
		if (!model_instance.isValid()) return;
		if (!m_project.hasComponent(model_instance, MODEL_INSTANCE_TYPE)) return;
		const Pose* parent_pose = lockPose(model_instance);
		if (!parent_pose) return;

		Transform parent_gameobject_transform = m_project.getTransform(bone_attachment.parent_gameobject);
		int idx = bone_attachment.bone_index;
		if (idx < 0 || idx > parent_pose->count)
		{
			unlockPose(model_instance, false);
			return;
		}
		float original_scale = m_project.getScale(bone_attachment.gameobject);
		Transform bone_transform = {parent_pose->positions[idx], parent_pose->rotations[idx], 1.0f};
		Transform relative_transform = { bone_attachment.relative_transform.pos, bone_attachment.relative_transform.rot, 1.0f};
		Transform result = parent_gameobject_transform * bone_transform * relative_transform;
		result.scale = original_scale;
		m_project.setTransform(bone_attachment.gameobject, result);
		unlockPose(model_instance, false);
	}


	GameObject getBoneAttachmentParent(GameObject gameobject) override
	{
		return m_bone_attachments[gameobject].parent_gameobject;
	}


	void updateRelativeMatrix(BoneAttachment& attachment)
	{
		if (attachment.parent_gameobject == INVALID_GAMEOBJECT) return;
		if (attachment.bone_index < 0) return;
		GameObject model_instance = attachment.parent_gameobject;
		if (!model_instance.isValid()) return;
		if (!m_project.hasComponent(model_instance, MODEL_INSTANCE_TYPE)) return;
		const Pose* pose = lockPose(model_instance);
		if (!pose) return;
		ASSERT(pose->is_absolute);
		if (attachment.bone_index >= pose->count)
		{
			unlockPose(model_instance, false);
			return;
		}
		Transform bone_transform = {pose->positions[attachment.bone_index], pose->rotations[attachment.bone_index], 1.0f};

		Transform inv_parent_transform = m_project.getTransform(attachment.parent_gameobject) * bone_transform;
		inv_parent_transform = inv_parent_transform.inverted();
		Transform child_transform = m_project.getTransform(attachment.gameobject);
		Transform res = inv_parent_transform * child_transform;
		attachment.relative_transform = {res.pos, res.rot};
		unlockPose(model_instance, false);
	}


	Vec3 getBoneAttachmentPosition(GameObject gameobject) override
	{
		return m_bone_attachments[gameobject].relative_transform.pos;
	}


	void setBoneAttachmentPosition(GameObject gameobject, const Vec3& pos) override
	{
		BoneAttachment& attachment = m_bone_attachments[gameobject];
		attachment.relative_transform.pos = pos;
		m_is_updating_attachments = true;
		updateBoneAttachment(attachment);
		m_is_updating_attachments = false;
	}


	Vec3 getBoneAttachmentRotation(GameObject gameobject) override
	{
		return m_bone_attachments[gameobject].relative_transform.rot.toEuler();
	}


	void setBoneAttachmentRotation(GameObject gameobject, const Vec3& rot) override
	{
		BoneAttachment& attachment = m_bone_attachments[gameobject];
		Vec3 euler = rot;
		euler.x = Math::clamp(euler.x, -Math::PI * 0.5f, Math::PI * 0.5f);
		attachment.relative_transform.rot.fromEuler(euler);
		m_is_updating_attachments = true;
		updateBoneAttachment(attachment);
		m_is_updating_attachments = false;
	}


	void setBoneAttachmentRotationQuat(GameObject gameobject, const Quat& rot) override
	{
		BoneAttachment& attachment = m_bone_attachments[gameobject];
		attachment.relative_transform.rot = rot;
		m_is_updating_attachments = true;
		updateBoneAttachment(attachment);
		m_is_updating_attachments = false;
	}


	int getBoneAttachmentBone(GameObject gameobject) override
	{
		return m_bone_attachments[gameobject].bone_index;
	}


	void setBoneAttachmentBone(GameObject gameobject, int value) override
	{
		BoneAttachment& ba = m_bone_attachments[gameobject];
		ba.bone_index = value;
		updateRelativeMatrix(ba);
	}


	void setBoneAttachmentParent(GameObject gameobject, GameObject parent) override
	{
		BoneAttachment& ba = m_bone_attachments[gameobject];
		ba.parent_gameobject = parent;
		if (parent.isValid() && parent.index < m_model_instances.size())
		{
			ModelInstance& mi = m_model_instances[parent.index];
			mi.flags.set(ModelInstance::IS_BONE_ATTACHMENT_PARENT);
		}
		updateRelativeMatrix(ba);
	}

	void startGame() override
	{
		m_is_game_running = true;
	}


	void stopGame() override
	{
		m_is_game_running = false;
	}


	void update(float dt, bool paused) override
	{
		PROFILE_FUNCTION();

		m_time += dt;
		for (int i = m_debug_triangles.size() - 1; i >= 0; --i)
		{
			float life = m_debug_triangles[i].life;
			if (life < 0)
			{
				m_debug_triangles.eraseFast(i);
			}
			else
			{
				life -= dt;
				m_debug_triangles[i].life = life;
			}
		}

		for(int i = m_debug_lines.size() - 1; i >= 0; --i)
		{
			float life = m_debug_lines[i].life;
			if(life < 0)
			{
				m_debug_lines.eraseFast(i);
			}
			else
			{
				life -= dt;
				m_debug_lines[i].life = life;
			}
		}


		for (int i = m_debug_points.size() - 1; i >= 0; --i)
		{
			float life = m_debug_points[i].life;
			if (life < 0)
			{
				m_debug_points.eraseFast(i);
			}
			else
			{
				life -= dt;
				m_debug_points[i].life = life;
			}
		}

		if (m_is_game_running && !paused)
		{
			for (auto* emitter : m_particle_emitters)
			{
				if (emitter->m_is_valid) emitter->update(dt);
			}
			for (auto* emitter : m_scripted_particle_emitters)
			{
				emitter->update(dt);
			}
		}
	}


	void serializeModelInstance(ISerializer& serialize, GameObject gameobject)
	{
		ModelInstance& r = m_model_instances[gameobject.index];
		ASSERT(r.gameobject != INVALID_GAMEOBJECT);

		serialize.write("source", r.model ? r.model->getPath().c_str() : "");
		serialize.write("flags", u8(r.flags.base & ModelInstance::PERSISTENT_FLAGS));
		bool has_changed_materials = r.model && r.model->isReady() && r.meshes != &r.model->getMesh(0);
		serialize.write("custom_materials", has_changed_materials ? r.mesh_count : 0);
		if (has_changed_materials)
		{
			for (int i = 0; i < r.mesh_count; ++i)
			{
				serialize.write("", r.meshes[i].material->getPath().c_str());
			}
		}
	}


	static bool hasCustomMeshes(ModelInstance& r)
	{
		return r.flags.isSet(ModelInstance::CUSTOM_MESHES);
	}


	void deserializeModelInstance(IDeserializer& serializer, GameObject gameobject, int scene_version)
	{
		while (gameobject.index >= m_model_instances.size())
		{
			auto& r = m_model_instances.emplace();
			r.gameobject = INVALID_GAMEOBJECT;
			r.pose = nullptr;
			r.model = nullptr;
			r.meshes = nullptr;
			r.mesh_count = 0;
		}
		auto& r = m_model_instances[gameobject.index];
		r.gameobject = gameobject;
		r.model = nullptr;
		r.pose = nullptr;
		r.flags.clear();
		r.flags.set(ModelInstance::ENABLED);
		r.meshes = nullptr;
		r.mesh_count = 0;

		r.matrix = m_project.getMatrix(r.gameobject);

		char path[MAX_PATH_LENGTH];
		serializer.read(path, lengthOf(path));
		if (scene_version > (int)RenderSceneVersion::MODEL_INSTNACE_FLAGS)
		{
			serializer.read(&r.flags.base);
			r.flags.base &= ModelInstance::PERSISTENT_FLAGS;

			if (scene_version <= (int)RenderSceneVersion::MODEL_INSTANCE_ENABLE)
			{
				r.flags.set(ModelInstance::ENABLED);
			}
		}

		if (path[0] != 0)
		{
			auto* model = static_cast<Model*>(m_engine.getResourceManager().get(Model::TYPE)->load(Path(path)));
			setModel(r.gameobject, model);
		}

		int material_count;
		serializer.read(&material_count);
		if (material_count > 0)
		{
			allocateCustomMeshes(r, material_count);
			for (int j = 0; j < material_count; ++j)
			{
				char path[MAX_PATH_LENGTH];
				serializer.read(path, lengthOf(path));
				setModelInstanceMaterial(r.gameobject, j, Path(path));
			}
		}

		m_project.onComponentCreated(r.gameobject, MODEL_INSTANCE_TYPE, this);
	}


	void serializeGlobalLight(ISerializer& serializer, GameObject gameobject)
	{
		GlobalLight& light = m_global_lights[gameobject];
		serializer.write("cascades", light.m_cascades);
		serializer.write("diffuse_color", light.m_diffuse_color);
		serializer.write("diffuse_intensity", light.m_diffuse_intensity);
		serializer.write("indirect_intensity", light.m_indirect_intensity);
		serializer.write("fog_bottom", light.m_fog_bottom);
		serializer.write("fog_color", light.m_fog_color);
		serializer.write("fog_density", light.m_fog_density);
		serializer.write("fog_height", light.m_fog_height);
	}


	void deserializeGlobalLight(IDeserializer& serializer, GameObject gameobject, int scene_version)
	{
		GlobalLight light;
		light.m_gameobject = gameobject;
		serializer.read(&light.m_cascades);
		if (scene_version <= (int)RenderSceneVersion::GLOBAL_LIGHT_REFACTOR)
		{
			int dummy;
			serializer.read(&dummy);
		}
		serializer.read(&light.m_diffuse_color);
		serializer.read(&light.m_diffuse_intensity);
		if (scene_version > (int)RenderSceneVersion::INDIRECT_INTENSITY)
		{
			serializer.read(&light.m_indirect_intensity);
		}
		else
		{
			light.m_indirect_intensity = 1;
		}
		serializer.read(&light.m_fog_bottom);
		serializer.read(&light.m_fog_color);
		serializer.read(&light.m_fog_density);
		serializer.read(&light.m_fog_height);
		m_global_lights.insert(gameobject, light);
		m_project.onComponentCreated(light.m_gameobject, GLOBAL_LIGHT_TYPE, this);
		m_active_global_light_gameobject = gameobject;
	}
	
	
	void serializePointLight(ISerializer& serializer, GameObject gameobject)
	{
		PointLight& light = m_point_lights[m_point_lights_map[gameobject]];
		serializer.write("attenuation", light.m_attenuation_param);
		serializer.write("cast_shadow", light.m_cast_shadows);
		serializer.write("diffuse_color", light.m_diffuse_color);
		serializer.write("diffuse_intensity", light.m_diffuse_intensity);
		serializer.write("fov", light.m_fov);
		serializer.write("range", light.m_range);
		serializer.write("specular_color", light.m_specular_color);
		serializer.write("specular_intensity", light.m_specular_intensity);
	}


	void deserializePointLight(IDeserializer& serializer, GameObject gameobject, int scene_version)
	{
		m_light_influenced_geometry.emplace(m_allocator);
		PointLight& light = m_point_lights.emplace();
		light.m_gameobject = gameobject;
		serializer.read(&light.m_attenuation_param);
		serializer.read(&light.m_cast_shadows);
		
		if (scene_version <= (int)RenderSceneVersion::POINT_LIGHT_NO_COMPONENT)
		{
			int dummy;
			serializer.read(&dummy);
		}
		serializer.read(&light.m_diffuse_color);
		serializer.read(&light.m_diffuse_intensity);
		serializer.read(&light.m_fov);
		serializer.read(&light.m_range);
		serializer.read(&light.m_specular_color);
		serializer.read(&light.m_specular_intensity);
		m_point_lights_map.insert(light.m_gameobject, m_point_lights.size() - 1);

		m_project.onComponentCreated(light.m_gameobject, POINT_LIGHT_TYPE, this);
	}


	void serializeDecal(ISerializer& serializer, GameObject gameobject)
	{
		const Decal& decal = m_decals[gameobject];
		serializer.write("scale", decal.scale);
		serializer.write("material", decal.material ? decal.material->getPath().c_str() : "");
	}


	void deserializeDecal(IDeserializer& serializer, GameObject gameobject, int /*scene_version*/)
	{
		ResourceManagerBase* material_manager = m_engine.getResourceManager().get(Material::TYPE);
		Decal& decal = m_decals.insert(gameobject);
		char tmp[MAX_PATH_LENGTH];
		decal.gameobject = gameobject;
		serializer.read(&decal.scale);
		serializer.read(tmp, lengthOf(tmp));
		decal.material = tmp[0] == '\0' ? nullptr : static_cast<Material*>(material_manager->load(Path(tmp)));
		updateDecalInfo(decal);
		m_project.onComponentCreated(decal.gameobject, DECAL_TYPE, this);
	}


	void serializeTextMesh(ISerializer& serializer, GameObject gameobject)
	{
		TextMesh& text = *m_text_meshes.get(gameobject);
		serializer.write("font", text.getFontResource() ? text.getFontResource()->getPath().c_str() : "");
		serializer.write("color", text.color);
		serializer.write("font_size", text.getFontSize());
		serializer.write("text", text.text.c_str());
	}


	void setTextMeshText(GameObject gameobject, const char* text) override
	{
		m_text_meshes.get(gameobject)->text = text;
	}


	const char* getTextMeshText(GameObject gameobject) override
	{
		return m_text_meshes.get(gameobject)->text.c_str();
	}


	bool isTextMeshCameraOriented(GameObject gameobject) override
	{
		TextMesh& text = *m_text_meshes.get(gameobject);
		return text.m_flags.isSet(TextMesh::CAMERA_ORIENTED);
	}


	void setTextMeshCameraOriented(GameObject gameobject, bool is_oriented) override
	{
		TextMesh& text = *m_text_meshes.get(gameobject);
		text.m_flags.set(TextMesh::CAMERA_ORIENTED, is_oriented);
	}


	void setTextMeshFontSize(GameObject gameobject, int value) override
	{
		TextMesh& text = *m_text_meshes.get(gameobject);
		text.setFontSize(value);
	}


	int getTextMeshFontSize(GameObject gameobject) override
	{
		return m_text_meshes.get(gameobject)->getFontSize();
	}


	static Vec4 ABGRu32ToRGBAVec4(u32 value)
	{
		float inv = 1 / 255.0f;
		return {
			((value >> 0) & 0xFF) * inv,
			((value >> 8) & 0xFF) * inv,
			((value >> 16) & 0xFF) * inv,
			((value >> 24) & 0xFF) * inv,
		};
	}


	static u32 RGBAVec4ToABGRu32(const Vec4& value)
	{
		u8 r = u8(value.x * 255 + 0.5f);
		u8 g = u8(value.y * 255 + 0.5f);
		u8 b = u8(value.z * 255 + 0.5f);
		u8 a = u8(value.w * 255 + 0.5f);
		return (a << 24) + (b << 16) + (g << 8) + r;
	}


	Vec4 getTextMeshColorRGBA(GameObject gameobject) override
	{
		return ABGRu32ToRGBAVec4(m_text_meshes.get(gameobject)->color);
	}


	void setTextMeshColorRGBA(GameObject gameobject, const Vec4& color) override
	{
		m_text_meshes.get(gameobject)->color = RGBAVec4ToABGRu32(color);
	}


	Path getTextMeshFontPath(GameObject gameobject) override
	{
		TextMesh& text = *m_text_meshes.get(gameobject);
		return text.getFontResource() == nullptr ? Path() : text.getFontResource()->getPath();
	}


	void getTextMeshesVertices(Array<TextMeshVertex>& vertices, GameObject camera) override
	{
		Matrix camera_mtx = m_project.getMatrix(camera);
		Vec3 cam_right = camera_mtx.getXVector();
		Vec3 cam_up = -camera_mtx.getYVector();
		for (int j = 0, nj = m_text_meshes.size(); j < nj; ++j)
		{
			TextMesh& text = *m_text_meshes.at(j);
			Font* font = text.getFont();
			if (!font) font = m_renderer.getFontManager().getDefaultFont();
			GameObject gameobject = m_text_meshes.getKey(j);
			const char* str = text.text.c_str();
			Vec3 base = m_project.getPosition(gameobject);
			Quat rot = m_project.getRotation(gameobject);
			float scale = m_project.getScale(gameobject);
			Vec3 right = rot.rotate({ 1, 0, 0 }) * scale;
			Vec3 up = rot.rotate({ 0, -1, 0 }) * scale;
			if (text.m_flags.isSet(TextMesh::CAMERA_ORIENTED))
			{
				right = cam_right * scale;
				up = cam_up * scale;
			}
			u32 color = text.color;
			Vec2 text_size = font->CalcTextSizeA((float)text.getFontSize(), FLT_MAX, 0, str);
			base += right * text_size.x * -0.5f;
			base += up * text_size.y * -0.5f;
			for (int i = 0, n = text.text.length(); i < n; ++i)
			{
				const Font::Glyph* glyph = font->FindGlyph(str[i]);
				if (!glyph) continue;

				Vec3 x0y0 = base + right * glyph->X0 + up * glyph->Y0;
				Vec3 x1y0 = base + right * glyph->X1 + up * glyph->Y0;
				Vec3 x1y1 = base + right * glyph->X1 + up * glyph->Y1;
				Vec3 x0y1 = base + right * glyph->X0 + up * glyph->Y1;

				vertices.push({ x0y0, color, { glyph->U0, glyph->V0 } });
				vertices.push({ x1y0, color, { glyph->U1, glyph->V0 } });
				vertices.push({ x1y1, color, { glyph->U1, glyph->V1 } });

				vertices.push({ x0y0, color, { glyph->U0, glyph->V0 } });
				vertices.push({ x1y1, color, { glyph->U1, glyph->V1 } });
				vertices.push({ x0y1, color, { glyph->U0, glyph->V1 } });
				
				base += right * glyph->XAdvance;
			}

		}
	}


	void setTextMeshFontPath(GameObject gameobject, const Path& path) override
	{
		TextMesh& text = *m_text_meshes.get(gameobject);
		FontManager& manager = m_renderer.getFontManager();
		FontResource* res = path.isValid() ? (FontResource*)manager.load(path) : nullptr;
		text.setFontResource(res);
	}

	
	void deserializeTextMesh(IDeserializer& serializer, GameObject gameobject, int /*scene_version*/)
	{
		TextMesh& text = *MALMY_NEW(m_allocator, TextMesh)(m_allocator);
		m_text_meshes.insert(gameobject, &text);

		char tmp[MAX_PATH_LENGTH];
		serializer.read(tmp, lengthOf(tmp));
		serializer.read(&text.color);
		int font_size;
		serializer.read(&font_size);
		text.setFontSize(font_size);
		serializer.read(&text.text);
		FontManager& manager = m_renderer.getFontManager();
		FontResource* res = tmp[0] ? (FontResource*)manager.load(Path(tmp)) : nullptr;
		text.setFontResource(res);
		m_project.onComponentCreated(gameobject, TEXT_MESH_TYPE, this);
	}


	void serializeCamera(ISerializer& serialize, GameObject gameobject)
	{
		Camera& camera = m_cameras[gameobject];
		serialize.write("far", camera.far);
		serialize.write("fov", camera.fov);
		serialize.write("is_ortho", camera.is_ortho);
		serialize.write("ortho_size", camera.ortho_size);
		serialize.write("near", camera.near);
		serialize.write("slot", camera.slot);
	}


	void deserializeCamera(IDeserializer& serializer, GameObject gameobject, int /*scene_version*/)
	{
		Camera camera;
		camera.gameobject = gameobject;
		serializer.read(&camera.far);
		serializer.read(&camera.fov);
		serializer.read(&camera.is_ortho);
		serializer.read(&camera.ortho_size);
		serializer.read(&camera.near);
		serializer.read(camera.slot, lengthOf(camera.slot));
		m_cameras.insert(camera.gameobject, camera);
		m_project.onComponentCreated(camera.gameobject, CAMERA_TYPE, this);
	}


	void serializeBoneAttachment(ISerializer& serializer, GameObject gameobject) 
	{
		BoneAttachment& attachment = m_bone_attachments[gameobject];
		serializer.write("bone_index", attachment.bone_index);
		serializer.write("parent", attachment.parent_gameobject);
		serializer.write("relative_transform", attachment.relative_transform);
	}


	void deserializeBoneAttachment(IDeserializer& serializer, GameObject gameobject, int /*scene_version*/)
	{
		BoneAttachment& bone_attachment = m_bone_attachments.emplace(gameobject);
		bone_attachment.gameobject = gameobject;
		serializer.read(&bone_attachment.bone_index);
		serializer.read(&bone_attachment.parent_gameobject);
		serializer.read(&bone_attachment.relative_transform);
		m_project.onComponentCreated(bone_attachment.gameobject, BONE_ATTACHMENT_TYPE, this);
		GameObject parent_gameobject = bone_attachment.parent_gameobject;
		if (parent_gameobject.isValid() && parent_gameobject.index < m_model_instances.size())
		{
			ModelInstance& mi = m_model_instances[parent_gameobject.index];
			mi.flags.set(ModelInstance::IS_BONE_ATTACHMENT_PARENT);
		}
	}


	void serializeTerrain(ISerializer& serializer, GameObject gameobject)
	{
		Terrain* terrain = m_terrains[gameobject];
		serializer.write("layer_mask", terrain->m_layer_mask);
		serializer.write("scale", terrain->m_scale);
		serializer.write("material", terrain->m_material ? terrain->m_material->getPath().c_str() : "");
		serializer.write("grass_count", terrain->m_grass_types.size());
		for (Terrain::GrassType& type : terrain->m_grass_types)
		{
			serializer.write("density", type.m_density);
			serializer.write("distance", type.m_distance);
			serializer.write("rotation_mode", (int)type.m_rotation_mode);
			serializer.write("model", type.m_grass_model ? type.m_grass_model->getPath().c_str() : "");
		}
	}

	void deserializeTerrain(IDeserializer& serializer, GameObject gameobject, int version)
	{
		Terrain* terrain = MALMY_NEW(m_allocator, Terrain)(m_renderer, gameobject, *this, m_allocator);
		m_terrains.insert(gameobject, terrain);
		terrain->m_gameobject = gameobject;
		serializer.read(&terrain->m_layer_mask);
		serializer.read(&terrain->m_scale);
		char tmp[MAX_PATH_LENGTH];
		serializer.read(tmp, lengthOf(tmp));
		auto* material = tmp[0] ? m_engine.getResourceManager().get(Material::TYPE)->load(Path(tmp)) : nullptr;
		terrain->setMaterial((Material*)material);

		int count;
		serializer.read(&count);
		for(int i = 0; i < count; ++i)
		{
			Terrain::GrassType type(*terrain);
			serializer.read(&type.m_density);
			serializer.read(&type.m_distance);
			if (version >= (int)RenderSceneVersion::GRASS_ROTATION_MODE)
			{
				serializer.read((int*)&type.m_rotation_mode);
			}
			type.m_idx = i;
			serializer.read(tmp, lengthOf(tmp));
			terrain->m_grass_types.push(type);
			terrain->setGrassTypePath(terrain->m_grass_types.size() - 1, Path(tmp));
		}

		m_project.onComponentCreated(gameobject, TERRAIN_TYPE, this);
	}

	void serializeEnvironmentProbe(ISerializer& serializer, GameObject gameobject) 
	{
		EnvironmentProbe& probe = m_environment_probes[gameobject];
		serializer.write("guid", probe.guid);
		serializer.write("flags", probe.flags.base);
		serializer.write("radiance_size", probe.radiance_size);
		serializer.write("irradiance_size", probe.irradiance_size);
		serializer.write("reflection_size", probe.reflection_size);
	}


	int getVersion() const override { return (int)RenderSceneVersion::LATEST; }


	void deserializeEnvironmentProbe(IDeserializer& serializer, GameObject gameobject, int scene_version)
	{
		auto* texture_manager = m_engine.getResourceManager().get(Texture::TYPE);
		StaticString<MAX_PATH_LENGTH> probe_dir("projects/", m_project.getName(), "/probes/");
		EnvironmentProbe& probe = m_environment_probes.insert(gameobject);
		serializer.read(&probe.guid);
		if (scene_version > (int)RenderSceneVersion::ENVIRONMENT_PROBE_FLAGS)
		{
			serializer.read(&probe.flags.base);
			serializer.read(&probe.radiance_size);
			serializer.read(&probe.irradiance_size);
			serializer.read(&probe.reflection_size);
		}

		StaticString<MAX_PATH_LENGTH> path_str(probe_dir, probe.guid, ".dds");
		
		probe.texture = nullptr;
		if (probe.flags.isSet(EnvironmentProbe::REFLECTION))
		{
			probe.texture = static_cast<Texture*>(texture_manager->load(Path(path_str)));
			probe.texture->setFlag(BGFX_TEXTURE_SRGB, true);
		}
		
		StaticString<MAX_PATH_LENGTH> irr_path_str(probe_dir, probe.guid, "_irradiance.dds");
		probe.irradiance = static_cast<Texture*>(texture_manager->load(Path(irr_path_str)));
		probe.irradiance->setFlag(BGFX_TEXTURE_SRGB, true);
		probe.irradiance->setFlag(BGFX_TEXTURE_MIN_ANISOTROPIC, true);
		probe.irradiance->setFlag(BGFX_TEXTURE_MAG_ANISOTROPIC, true);
		StaticString<MAX_PATH_LENGTH> r_path_str(probe_dir, probe.guid, "_radiance.dds");
		probe.radiance = static_cast<Texture*>(texture_manager->load(Path(r_path_str)));
		probe.radiance->setFlag(BGFX_TEXTURE_SRGB, true);
		probe.radiance->setFlag(BGFX_TEXTURE_MIN_ANISOTROPIC, true);
		probe.radiance->setFlag(BGFX_TEXTURE_MAG_ANISOTROPIC, true);

		m_project.onComponentCreated(gameobject, ENVIRONMENT_PROBE_TYPE, this);
	}


	void serializeScriptedParticleEmitter(ISerializer& serializer, GameObject gameobject)
	{
		ScriptedParticleEmitter* emitter = m_scripted_particle_emitters[gameobject];
		const Material* material = emitter->getMaterial();
		serializer.write("material", material ? material->getPath().c_str() : "");
	}


	void deserializeScriptedParticleEmitter(IDeserializer& serializer, GameObject gameobject, int scene_version)
	{
		ScriptedParticleEmitter* emitter = MALMY_NEW(m_allocator, ScriptedParticleEmitter)(gameobject, m_allocator);
		emitter->m_gameobject = gameobject;

		char tmp[MAX_PATH_LENGTH];
		serializer.read(tmp, lengthOf(tmp));
		ResourceManagerBase* material_manager = m_engine.getResourceManager().get(Material::TYPE);
		Material* material = (Material*)material_manager->load(Path(tmp));
		emitter->setMaterial(material);

		m_scripted_particle_emitters.insert(gameobject, emitter);
		m_project.onComponentCreated(gameobject, SCRIPTED_PARTICLE_EMITTER_TYPE, this);
	}


	void serializeParticleEmitter(ISerializer& serializer, GameObject gameobject)
	{
		ParticleEmitter* emitter = m_particle_emitters[gameobject];
		serializer.write("autoemit", emitter->m_autoemit);
		serializer.write("local_space", emitter->m_local_space);
		serializer.write("spawn_period_from", emitter->m_spawn_period.from);
		serializer.write("spawn_period_to", emitter->m_spawn_period.to);
		serializer.write("initial_life_from", emitter->m_initial_life.from);
		serializer.write("initial_life_to", emitter->m_initial_life.to);
		serializer.write("initial_size_from", emitter->m_initial_size.from);
		serializer.write("initial_size_to", emitter->m_initial_size.to);
		serializer.write("spawn_count_from", emitter->m_spawn_count.from);
		serializer.write("spawn_count_to", emitter->m_spawn_count.to);
		const Material* material = emitter->getMaterial();
		serializer.write("material", material ? material->getPath().c_str() : "");
	}


	void deserializeParticleEmitter(IDeserializer& serializer, GameObject gameobject, int scene_version)
	{
		ParticleEmitter* emitter = MALMY_NEW(m_allocator, ParticleEmitter)(gameobject, m_project, m_allocator);
		emitter->m_gameobject = gameobject;
		serializer.read(&emitter->m_autoemit);
		serializer.read(&emitter->m_local_space);
		serializer.read(&emitter->m_spawn_period.from);
		serializer.read(&emitter->m_spawn_period.to);
		serializer.read(&emitter->m_initial_life.from);
		serializer.read(&emitter->m_initial_life.to);
		serializer.read(&emitter->m_initial_size.from);
		serializer.read(&emitter->m_initial_size.to);
		serializer.read(&emitter->m_spawn_count.from);
		serializer.read(&emitter->m_spawn_count.to);
		if (scene_version > (int)RenderSceneVersion::EMITTER_MATERIAL)
		{
			char tmp[MAX_PATH_LENGTH];
			serializer.read(tmp, lengthOf(tmp));
			ResourceManagerBase* material_manager = m_engine.getResourceManager().get(Material::TYPE);
			Material* material = (Material*)material_manager->load(Path(tmp));
			emitter->setMaterial(material);
		}

		m_particle_emitters.insert(gameobject, emitter);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_TYPE, this);
	}

	void serializeParticleEmitterAlpha(ISerializer& serializer, GameObject gameobject)
	{
		ParticleEmitter* emitter = m_particle_emitters[gameobject];
		auto* module = (ParticleEmitter::AlphaModule*)emitter->getModule(PARTICLE_EMITTER_ALPHA_TYPE);
		serializer.write("count", module->m_values.size());
		for (Vec2 v : module->m_values)
		{
			serializer.write("", v.x);
			serializer.write("", v.y);
		}
	}
	
	
	void deserializeParticleEmitterAlpha(IDeserializer& serializer, GameObject gameobject, int /*scene_version*/)
	{
		int index = allocateParticleEmitter(gameobject);
		ParticleEmitter* emitter = m_particle_emitters.at(index);
		auto* module = MALMY_NEW(m_allocator, ParticleEmitter::AlphaModule)(*emitter);
		int count;
		serializer.read(&count);
		module->m_values.clear();
		for (int i = 0; i < count; ++i)
		{
			Vec2& v = module->m_values.emplace();
			serializer.read(&v.x);
			serializer.read(&v.y);
		}
		module->sample();
		emitter->addModule(module);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_ALPHA_TYPE, this);
	}


	void serializeParticleEmitterAttractor(ISerializer& serializer, GameObject gameobject)
	{
		ParticleEmitter* emitter = m_particle_emitters[gameobject];
		auto* module = (ParticleEmitter::AttractorModule*)emitter->getModule(PARTICLE_EMITTER_ATTRACTOR_TYPE);
		serializer.write("force", module->m_force);
		serializer.write("count", module->m_count);
		for (int i = 0; i < module->m_count; ++i)
		{
			serializer.write("", module->m_entities[i]);
		}
	}


	void deserializeParticleEmitterAttractor(IDeserializer& serializer, GameObject gameobject, int /*scene_version*/)
	{
		int index = allocateParticleEmitter(gameobject);
		ParticleEmitter* emitter = m_particle_emitters.at(index);

		auto* module = MALMY_NEW(m_allocator, ParticleEmitter::AttractorModule)(*emitter);
		serializer.read(&module->m_force);
		serializer.read(&module->m_count);
		for (int i = 0; i < module->m_count; ++i)
		{
			serializer.read(&module->m_entities[i]);
		}
		emitter->addModule(module);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_ATTRACTOR_TYPE, this);
	}

	void serializeParticleEmitterForce(ISerializer& serializer, GameObject gameobject)
	{
		ParticleEmitter* emitter = m_particle_emitters[gameobject];
		auto* module = (ParticleEmitter::ForceModule*)emitter->getModule(PARTICLE_EMITTER_FORCE_HASH);
		serializer.write("acceleration", module->m_acceleration);
	}


	void deserializeParticleEmitterForce(IDeserializer& serializer, GameObject gameobject, int /*scene_version*/)
	{
		int index = allocateParticleEmitter(gameobject);
		ParticleEmitter* emitter = m_particle_emitters.at(index);

		auto* module = MALMY_NEW(m_allocator, ParticleEmitter::ForceModule)(*emitter);
		serializer.read(&module->m_acceleration);
		emitter->addModule(module);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_FORCE_HASH, this);
	}


	void serializeParticleEmitterLinearMovement(ISerializer& serializer, GameObject gameobject)
	{
		ParticleEmitter* emitter = m_particle_emitters[gameobject];
		auto* module = (ParticleEmitter::LinearMovementModule*)emitter->getModule(PARTICLE_EMITTER_LINEAR_MOVEMENT_TYPE);
		serializer.write("x_from", module->m_x.from);
		serializer.write("x_to", module->m_x.to);
		serializer.write("y_from", module->m_y.from);
		serializer.write("y_to", module->m_y.to);
		serializer.write("z_from", module->m_z.from);
		serializer.write("z_to", module->m_z.to);
	}


	void deserializeParticleEmitterLinearMovement(IDeserializer& serializer, GameObject gameobject, int /*scene_version*/)
	{
		int index = allocateParticleEmitter(gameobject);
		ParticleEmitter* emitter = m_particle_emitters.at(index);

		auto* module = MALMY_NEW(m_allocator, ParticleEmitter::LinearMovementModule)(*emitter);
		serializer.read(&module->m_x.from);
		serializer.read(&module->m_x.to);
		serializer.read(&module->m_y.from);
		serializer.read(&module->m_y.to);
		serializer.read(&module->m_z.from);
		serializer.read(&module->m_z.to);
		emitter->addModule(module);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_LINEAR_MOVEMENT_TYPE, this);
	}


	void serializeParticleEmitterPlane(ISerializer& serializer, GameObject gameobject)
	{
		ParticleEmitter* emitter = m_particle_emitters[gameobject];
		auto* module = (ParticleEmitter::PlaneModule*)emitter->getModule(PARTICLE_EMITTER_PLANE_TYPE);
		serializer.write("bounce", module->m_bounce);
		serializer.write("entities_count", module->m_count);
		for (int i = 0; i < module->m_count; ++i)
		{
			serializer.write("", module->m_entities[i]);
		}
	}


	void deserializeParticleEmitterPlane(IDeserializer& serializer, GameObject gameobject, int /*scene_version*/)
	{
		int index = allocateParticleEmitter(gameobject);
		ParticleEmitter* emitter = m_particle_emitters.at(index);

		auto* module = MALMY_NEW(m_allocator, ParticleEmitter::PlaneModule)(*emitter);
		serializer.read(&module->m_bounce);
		serializer.read(&module->m_count);
		for (int i = 0; i < module->m_count; ++i)
		{
			serializer.read(&module->m_entities[i]);
		}
		emitter->addModule(module);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_PLANE_TYPE, this);
	}

	void serializeParticleEmitterSpawnShape(ISerializer& serializer, GameObject gameobject)
	{
		ParticleEmitter* emitter = m_particle_emitters[gameobject];
		auto* module = (ParticleEmitter::SpawnShapeModule*)emitter->getModule(PARTICLE_EMITTER_SPAWN_SHAPE_TYPE);
		serializer.write("shape", (u8)module->m_shape);
		serializer.write("radius", module->m_radius);
	}


	void deserializeParticleEmitterSpawnShape(IDeserializer& serializer, GameObject gameobject, int /*scene_version*/)
	{
		int index = allocateParticleEmitter(gameobject);
		ParticleEmitter* emitter = m_particle_emitters.at(index);

		auto* module = MALMY_NEW(m_allocator, ParticleEmitter::SpawnShapeModule)(*emitter);
		serializer.read((u8*)&module->m_shape);
		serializer.read(&module->m_radius);
		emitter->addModule(module);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_SPAWN_SHAPE_TYPE, this);
	}

	void serializeParticleEmitterSize(ISerializer& serializer, GameObject gameobject)
	{
		ParticleEmitter* emitter = m_particle_emitters[gameobject];
		auto* module = (ParticleEmitter::SizeModule*)emitter->getModule(PARTICLE_EMITTER_SIZE_TYPE);
		serializer.write("count", module->m_values.size());
		for (Vec2 v : module->m_values)
		{
			serializer.write("", v.x);
			serializer.write("", v.y);
		}
	}


	void deserializeParticleEmitterSize(IDeserializer& serializer, GameObject gameobject, int /*scene_version*/)
	{
		int index = allocateParticleEmitter(gameobject);
		ParticleEmitter* emitter = m_particle_emitters.at(index);

		auto* module = MALMY_NEW(m_allocator, ParticleEmitter::SizeModule)(*emitter);
		int count;
		serializer.read(&count);
		module->m_values.clear();
		for (int i = 0; i < count; ++i)
		{
			Vec2& v = module->m_values.emplace();
			serializer.read(&v.x);
			serializer.read(&v.y);
		}
		module->sample();
		emitter->addModule(module);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_SIZE_TYPE, this);
	}


	void serializeParticleEmitterRandomRotation(ISerializer& serialize, GameObject gameobject) {}


	void deserializeParticleEmitterRandomRotation(IDeserializer& serialize, GameObject gameobject, int /*scene_version*/)
	{
		int index = allocateParticleEmitter(gameobject);
		ParticleEmitter* emitter = m_particle_emitters.at(index);

		auto* module = MALMY_NEW(m_allocator, ParticleEmitter::RandomRotationModule)(*emitter);
		emitter->addModule(module);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_RANDOM_ROTATION_TYPE, this);
	}


	void serializeParticleEmitterSubimage(ISerializer& serializer, GameObject gameobject)
	{
		ParticleEmitter* emitter = m_particle_emitters[gameobject];
		auto* module = (ParticleEmitter::SubimageModule*)emitter->getModule(PARTICLE_EMITTER_SUBIMAGE_TYPE);
		serializer.write("rows", module->rows);
		serializer.write("cols", module->cols);
	}


	void deserializeParticleEmitterSubimage(IDeserializer& serializer, GameObject gameobject, int /*scene_version*/)
	{
		int index = allocateParticleEmitter(gameobject);
		ParticleEmitter* emitter = m_particle_emitters.at(index);

		auto* module = MALMY_NEW(m_allocator, ParticleEmitter::SubimageModule)(*emitter);
		serializer.read(&module->rows);
		serializer.read(&module->cols);
		emitter->addModule(module);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_SUBIMAGE_TYPE, this);
	}


	void serializeBoneAttachments(OutputBlob& serializer)
	{
		serializer.write((i32)m_bone_attachments.size());
		for (auto& attachment : m_bone_attachments)
		{
			serializer.write(attachment.bone_index);
			serializer.write(attachment.gameobject);
			serializer.write(attachment.parent_gameobject);
			serializer.write(attachment.relative_transform);
		}
	}

	void serializeCameras(OutputBlob& serializer)
	{
		serializer.write((i32)m_cameras.size());
		for (auto& camera : m_cameras)
		{
			serializer.write(camera.gameobject);
			serializer.write(camera.far);
			serializer.write(camera.fov);
			serializer.write(camera.is_ortho);
			serializer.write(camera.ortho_size);
			serializer.write(camera.near);
			serializer.writeString(camera.slot);
		}
	}

	void serializeLights(OutputBlob& serializer)
	{
		serializer.write((i32)m_point_lights.size());
		for (int i = 0, c = m_point_lights.size(); i < c; ++i)
		{
			serializer.write(m_point_lights[i]);
		}

		serializer.write((i32)m_global_lights.size());
		for (const GlobalLight& light : m_global_lights)
		{
			serializer.write(light);
		}
		serializer.write(m_active_global_light_gameobject);
	}

	void serializeModelInstances(OutputBlob& serializer)
	{
		serializer.write((i32)m_model_instances.size());
		for (auto& r : m_model_instances)
		{
			serializer.write(r.gameobject);
			serializer.write(u8(r.flags.base & ModelInstance::PERSISTENT_FLAGS));
			if(r.gameobject != INVALID_GAMEOBJECT)
			{
				serializer.write(r.model ? r.model->getPath().getHash() : 0);
				bool has_changed_materials = r.model && r.model->isReady() && r.meshes != &r.model->getMesh(0);
				serializer.write(has_changed_materials ? r.mesh_count : 0);
				if (has_changed_materials)
				{
					for (int i = 0; i < r.mesh_count; ++i)
					{
						serializer.writeString(r.meshes[i].material->getPath().c_str());
					}
				}
			}
			
		}
	}

	void serializeTerrains(OutputBlob& serializer)
	{
		serializer.write((i32)m_terrains.size());
		for (auto* terrain : m_terrains)
		{
			terrain->serialize(serializer);
		}
	}

	void serializeTextMeshes(OutputBlob& serializer)
	{
		serializer.write(m_text_meshes.size());
		for (int i = 0, n = m_text_meshes.size(); i < n; ++i)
		{
			TextMesh& text = *m_text_meshes.at(i);
			GameObject e = m_text_meshes.getKey(i);
			serializer.write(e);
			serializer.writeString(text.getFontResource() ? text.getFontResource()->getPath().c_str() : "");
			serializer.write(text.color);
			serializer.write(text.getFontSize());
			serializer.write(text.text);
		}
	}

	void deserializeTextMeshes(InputBlob& serializer)
	{
		int count;
		serializer.read(count);
		FontManager& manager = m_renderer.getFontManager();
		for (int i = 0; i < count; ++i)
		{
			GameObject e;
			serializer.read(e);
			TextMesh& text = *MALMY_NEW(m_allocator, TextMesh)(m_allocator);
			m_text_meshes.insert(e, &text);
			char tmp[MAX_PATH_LENGTH];
			serializer.readString(tmp, lengthOf(tmp));
			serializer.read(text.color);
			int font_size;
			serializer.read(font_size);
			text.setFontSize(font_size);
			serializer.read(text.text);
			FontResource* res = tmp[0] ? (FontResource*)manager.load(Path(tmp)) : nullptr;
			text.setFontResource(res);
			m_project.onComponentCreated(e, TEXT_MESH_TYPE, this);
		}
	}


	void deserializeDecals(InputBlob& serializer)
	{
		ResourceManagerBase* material_manager = m_engine.getResourceManager().get(Material::TYPE);
		int count;
		serializer.read(count);
		m_decals.reserve(count);
		for (int i = 0; i < count; ++i)
		{
			char tmp[MAX_PATH_LENGTH];
			Decal decal;
			serializer.read(decal.gameobject);
			serializer.read(decal.scale);
			serializer.readString(tmp, lengthOf(tmp));
			decal.material = tmp[0] == '\0' ? nullptr : static_cast<Material*>(material_manager->load(Path(tmp)));
			updateDecalInfo(decal);
			m_decals.insert(decal.gameobject, decal);
			m_project.onComponentCreated(decal.gameobject, DECAL_TYPE, this);
		}
	}


	void serializeDecals(OutputBlob& serializer)
	{
		serializer.write(m_decals.size());
		for (auto& decal : m_decals)
		{
			serializer.write(decal.gameobject);
			serializer.write(decal.scale);
			serializer.writeString(decal.material ? decal.material->getPath().c_str() : "");
		}
	}


	void serializeEnvironmentProbes(OutputBlob& serializer)
	{
		i32 count = m_environment_probes.size();
		serializer.write(count);
		for (int i = 0; i < count; ++i)
		{
			GameObject gameobject = m_environment_probes.getKey(i);
			serializer.write(gameobject);
			const EnvironmentProbe& probe = m_environment_probes.at(i);
			serializer.write(probe.guid);
			serializer.write(probe.flags.base);
			serializer.write(probe.radiance_size);
			serializer.write(probe.irradiance_size);
			serializer.write(probe.reflection_size);
		}
	}


	void deserializeEnvironmentProbes(InputBlob& serializer)
	{
		i32 count;
		serializer.read(count);
		m_environment_probes.reserve(count);
		auto* texture_manager = m_engine.getResourceManager().get(Texture::TYPE);
		StaticString<MAX_PATH_LENGTH> probe_dir("projects/", m_project.getName(), "/probes/");
		for (int i = 0; i < count; ++i)
		{
			GameObject gameobject;
			serializer.read(gameobject);
			EnvironmentProbe& probe = m_environment_probes.insert(gameobject);
			serializer.read(probe.guid);
			serializer.read(probe.flags.base);
			serializer.read(probe.radiance_size);
			serializer.read(probe.irradiance_size);
			serializer.read(probe.reflection_size);
			probe.texture = nullptr;
			if (probe.flags.isSet(EnvironmentProbe::REFLECTION))
			{
				StaticString<MAX_PATH_LENGTH> path_str(probe_dir, probe.guid, ".dds");
				probe.texture = static_cast<Texture*>(texture_manager->load(Path(path_str)));
				probe.texture->setFlag(BGFX_TEXTURE_SRGB, true);
			}
			StaticString<MAX_PATH_LENGTH> irr_path_str(probe_dir, probe.guid, "_irradiance.dds");
			probe.irradiance = static_cast<Texture*>(texture_manager->load(Path(irr_path_str)));
			probe.irradiance->setFlag(BGFX_TEXTURE_SRGB, true);
			probe.irradiance->setFlag(BGFX_TEXTURE_MIN_ANISOTROPIC, true);
			probe.irradiance->setFlag(BGFX_TEXTURE_MAG_ANISOTROPIC, true);
			StaticString<MAX_PATH_LENGTH> r_path_str(probe_dir, probe.guid, "_radiance.dds");
			probe.radiance = static_cast<Texture*>(texture_manager->load(Path(r_path_str)));
			probe.radiance->setFlag(BGFX_TEXTURE_SRGB, true);
			probe.radiance->setFlag(BGFX_TEXTURE_MIN_ANISOTROPIC, true);
			probe.radiance->setFlag(BGFX_TEXTURE_MAG_ANISOTROPIC, true);

			m_project.onComponentCreated(gameobject, ENVIRONMENT_PROBE_TYPE, this);
		}
	}


	void deserializeBoneAttachments(InputBlob& serializer)
	{
		i32 count;
		serializer.read(count);
		m_bone_attachments.clear();
		m_bone_attachments.reserve(count);
		for (int i = 0; i < count; ++i)
		{
			BoneAttachment bone_attachment;
			serializer.read(bone_attachment.bone_index);
			serializer.read(bone_attachment.gameobject);
			serializer.read(bone_attachment.parent_gameobject);
			serializer.read(bone_attachment.relative_transform);
			m_bone_attachments.insert(bone_attachment.gameobject, bone_attachment);
			m_project.onComponentCreated(bone_attachment.gameobject, BONE_ATTACHMENT_TYPE, this);
		}
	}


	void deserializeParticleEmitters(InputBlob& serializer)
	{
		int count;
		serializer.read(count);
		m_particle_emitters.reserve(count);
		for(int i = 0; i < count; ++i)
		{
			ParticleEmitter* emitter = MALMY_NEW(m_allocator, ParticleEmitter)(INVALID_GAMEOBJECT, m_project, m_allocator);
			serializer.read(emitter->m_is_valid);
			if (emitter->m_is_valid)
			{
				emitter->deserialize(serializer, m_engine.getResourceManager());
				if (emitter->m_is_valid) m_project.onComponentCreated(emitter->m_gameobject, PARTICLE_EMITTER_TYPE, this);
				for (auto* module : emitter->m_modules)
				{
					if (module->getType() == ParticleEmitter::AlphaModule::s_type)
					{
						m_project.onComponentCreated(emitter->m_gameobject, PARTICLE_EMITTER_ALPHA_TYPE, this);
					}
					else if (module->getType() == ParticleEmitter::ForceModule::s_type)
					{
						m_project.onComponentCreated(emitter->m_gameobject, PARTICLE_EMITTER_FORCE_HASH, this);
					}
					else if (module->getType() == ParticleEmitter::SubimageModule::s_type)
					{
						m_project.onComponentCreated(emitter->m_gameobject, PARTICLE_EMITTER_SUBIMAGE_TYPE, this);
					}
					else if (module->getType() == ParticleEmitter::SpawnShapeModule::s_type)
					{
						m_project.onComponentCreated(emitter->m_gameobject, PARTICLE_EMITTER_SPAWN_SHAPE_TYPE, this);
					}
					else if (module->getType() == ParticleEmitter::AttractorModule::s_type)
					{
						m_project.onComponentCreated(emitter->m_gameobject, PARTICLE_EMITTER_ATTRACTOR_TYPE, this);
					}
					else if (module->getType() == ParticleEmitter::LinearMovementModule::s_type)
					{
						m_project.onComponentCreated(emitter->m_gameobject, PARTICLE_EMITTER_LINEAR_MOVEMENT_TYPE, this);
					}
					else if (module->getType() == ParticleEmitter::PlaneModule::s_type)
					{
						m_project.onComponentCreated(emitter->m_gameobject, PARTICLE_EMITTER_PLANE_TYPE, this);
					}
					else if (module->getType() == ParticleEmitter::RandomRotationModule::s_type)
					{
						m_project.onComponentCreated(emitter->m_gameobject, PARTICLE_EMITTER_RANDOM_ROTATION_TYPE, this);
					}
					else if (module->getType() == ParticleEmitter::SizeModule::s_type)
					{
						m_project.onComponentCreated(emitter->m_gameobject, PARTICLE_EMITTER_SIZE_TYPE, this);
					}
				}
			}
			if (!emitter->m_is_valid && emitter->m_modules.empty())
			{
				MALMY_DELETE(m_allocator, emitter);
			}
			else
			{
				m_particle_emitters.insert(emitter->m_gameobject, emitter);
			}
		}

		serializer.read(count);
		m_scripted_particle_emitters.reserve(count);
		for (int i = 0; i < count; ++i)
		{
			ScriptedParticleEmitter* emitter = MALMY_NEW(m_allocator, ScriptedParticleEmitter)(INVALID_GAMEOBJECT, m_allocator);
			emitter->deserialize(serializer, m_engine.getResourceManager());
			m_scripted_particle_emitters.insert(emitter->m_gameobject, emitter);
			m_project.onComponentCreated(emitter->m_gameobject, SCRIPTED_PARTICLE_EMITTER_TYPE, this);
		}
	}


	void serializeParticleEmitters(OutputBlob& serializer)
	{
		serializer.write(m_particle_emitters.size());
		for (auto* emitter : m_particle_emitters)
		{
			serializer.write(emitter->m_is_valid);
			emitter->serialize(serializer);
		}

		serializer.write(m_scripted_particle_emitters.size());
		for (auto* emitter : m_scripted_particle_emitters)
		{
			emitter->serialize(serializer);
		}
	}


	void serialize(OutputBlob& serializer) override
	{
		serializeCameras(serializer);
		serializeModelInstances(serializer);
		serializeLights(serializer);
		serializeTerrains(serializer);
		serializeParticleEmitters(serializer);
		serializeBoneAttachments(serializer);
		serializeEnvironmentProbes(serializer);
		serializeDecals(serializer);
		serializeTextMeshes(serializer);
	}


	void deserializeCameras(InputBlob& serializer)
	{
		i32 size;
		serializer.read(size);
		m_cameras.rehash(size);
		for (int i = 0; i < size; ++i)
		{
			Camera camera;
			serializer.read(camera.gameobject);
			serializer.read(camera.far);
			serializer.read(camera.fov);
			serializer.read(camera.is_ortho);
			serializer.read(camera.ortho_size);
			serializer.read(camera.near);
			serializer.readString(camera.slot, lengthOf(camera.slot));

			m_cameras.insert(camera.gameobject, camera);
			m_project.onComponentCreated(camera.gameobject, CAMERA_TYPE, this);
		}
	}

	void deserializeModelInstances(InputBlob& serializer)
	{
		i32 size = 0;
		serializer.read(size);
		m_model_instances.reserve(size);
		for (int i = 0; i < size; ++i)
		{
			auto& r = m_model_instances.emplace();
			serializer.read(r.gameobject);
			serializer.read(r.flags);
			r.flags.base &= ModelInstance::PERSISTENT_FLAGS;
			ASSERT(r.gameobject.index == i || !r.gameobject.isValid());
			r.model = nullptr;
			r.pose = nullptr;
			r.meshes = nullptr;
			r.mesh_count = 0;

			if(r.gameobject != INVALID_GAMEOBJECT)
			{
				r.matrix = m_project.getMatrix(r.gameobject);

				u32 path;
				serializer.read(path);

				if (path != 0)
				{
					auto* model = static_cast<Model*>(m_engine.getResourceManager().get(Model::TYPE)->load(Path(path)));
					setModel(r.gameobject, model);
				}

				int material_count;
				serializer.read(material_count);
				if (material_count > 0)
				{
					allocateCustomMeshes(r, material_count);
					for (int j = 0; j < material_count; ++j)
					{
						char path[MAX_PATH_LENGTH];
						serializer.readString(path, lengthOf(path));
						setModelInstanceMaterial(r.gameobject, j, Path(path));
					}
				}

				m_project.onComponentCreated(r.gameobject, MODEL_INSTANCE_TYPE, this);
			}
		}
	}

	void deserializeLights(InputBlob& serializer)
	{
		i32 size = 0;
		serializer.read(size);
		m_point_lights.resize(size);
		for (int i = 0; i < size; ++i)
		{
			m_light_influenced_geometry.emplace(m_allocator);
			PointLight& light = m_point_lights[i];
			serializer.read(light);
			m_point_lights_map.insert(light.m_gameobject, i);

			m_project.onComponentCreated(light.m_gameobject, POINT_LIGHT_TYPE, this);
		}

		serializer.read(size);
		for (int i = 0; i < size; ++i)
		{
			GlobalLight light;
			serializer.read(light);
			m_global_lights.insert(light.m_gameobject, light);
			m_project.onComponentCreated(light.m_gameobject, GLOBAL_LIGHT_TYPE, this);
		}
		serializer.read(m_active_global_light_gameobject);
	}

	void deserializeTerrains(InputBlob& serializer)
	{
		i32 size = 0;
		serializer.read(size);
		for (int i = 0; i < size; ++i)
		{
			auto* terrain = MALMY_NEW(m_allocator, Terrain)(m_renderer, INVALID_GAMEOBJECT, *this, m_allocator);
			terrain->deserialize(serializer, m_project, *this);
			m_terrains.insert(terrain->getGameObject(), terrain);
		}
	}


	void deserialize(InputBlob& serializer) override
	{
		deserializeCameras(serializer);
		deserializeModelInstances(serializer);
		deserializeLights(serializer);
		deserializeTerrains(serializer);
		deserializeParticleEmitters(serializer);
		deserializeBoneAttachments(serializer);
		deserializeEnvironmentProbes(serializer);
		deserializeDecals(serializer);
		deserializeTextMeshes(serializer);
	}


	void destroyBoneAttachment(GameObject gameobject)
	{
		const BoneAttachment& bone_attachment = m_bone_attachments[gameobject];
		GameObject parent_gameobject = bone_attachment.parent_gameobject;
		if (parent_gameobject.isValid() && parent_gameobject.index < m_model_instances.size())
		{
			ModelInstance& mi = m_model_instances[bone_attachment.parent_gameobject.index];
			mi.flags.unset(ModelInstance::IS_BONE_ATTACHMENT_PARENT);
		}
		m_bone_attachments.erase(gameobject);
		m_project.onComponentDestroyed(gameobject, BONE_ATTACHMENT_TYPE, this);
	}


	void destroyEnvironmentProbe(GameObject gameobject)
	{
		auto& probe = m_environment_probes[gameobject];
		if (probe.texture) probe.texture->getResourceManager().unload(*probe.texture);
		if (probe.irradiance) probe.irradiance->getResourceManager().unload(*probe.irradiance);
		if (probe.radiance) probe.radiance->getResourceManager().unload(*probe.radiance);
		m_environment_probes.erase(gameobject);
		m_project.onComponentDestroyed(gameobject, ENVIRONMENT_PROBE_TYPE, this);
	}


	void destroyModelInstance(GameObject gameobject)
	{
		for (int i = 0; i < m_light_influenced_geometry.size(); ++i)
		{
			Array<GameObject>& influenced_geometry = m_light_influenced_geometry[i];
			for (int j = 0; j < influenced_geometry.size(); ++j)
			{
				if (influenced_geometry[j] == gameobject)
				{
					influenced_geometry.erase(j);
					break;
				}
			}
		}

		setModel(gameobject, nullptr);
		auto& model_instance = m_model_instances[gameobject.index];
		MALMY_DELETE(m_allocator, model_instance.pose);
		model_instance.pose = nullptr;
		model_instance.gameobject = INVALID_GAMEOBJECT;
		m_project.onComponentDestroyed(gameobject, MODEL_INSTANCE_TYPE, this);
	}


	void destroyGlobalLight(GameObject gameobject)
	{
		m_project.onComponentDestroyed(gameobject, GLOBAL_LIGHT_TYPE, this);

		if (gameobject == m_active_global_light_gameobject)
		{
			m_active_global_light_gameobject = INVALID_GAMEOBJECT;
		}
		m_global_lights.erase(gameobject);
	}


	void destroyDecal(GameObject gameobject)
	{
		m_decals.erase(gameobject);
		m_project.onComponentDestroyed(gameobject, DECAL_TYPE, this);
	}


	void destroyPointLight(GameObject gameobject)
	{
		int index = m_point_lights_map[gameobject];
		m_point_lights.eraseFast(index);
		m_point_lights_map.erase(gameobject);
		m_light_influenced_geometry.eraseFast(index);
		if (index < m_point_lights.size())
		{
			m_point_lights_map[{m_point_lights[index].m_gameobject.index}] = index;
		}
		m_project.onComponentDestroyed(gameobject, POINT_LIGHT_TYPE, this);
	}


	void destroyTextMesh(GameObject gameobject)
	{
		TextMesh* text = m_text_meshes[gameobject];
		MALMY_DELETE(m_allocator, text);
		m_text_meshes.erase(gameobject);
		m_project.onComponentDestroyed(gameobject, TEXT_MESH_TYPE, this);
	}


	void destroyCamera(GameObject gameobject)
	{
		m_cameras.erase(gameobject);
		m_project.onComponentDestroyed(gameobject, CAMERA_TYPE, this);
	}


	void destroyTerrain(GameObject gameobject)
	{
		MALMY_DELETE(m_allocator, m_terrains[gameobject]);
		m_terrains.erase(gameobject);
		m_project.onComponentDestroyed(gameobject, TERRAIN_TYPE, this);
	}


	void destroyParticleEmitter(GameObject gameobject)
	{
		auto* emitter = m_particle_emitters[gameobject];
		emitter->reset();
		emitter->m_is_valid = false;
		m_project.onComponentDestroyed(emitter->m_gameobject, PARTICLE_EMITTER_TYPE, this);
		cleanup(emitter);
	}


	void destroyScriptedParticleEmitter(GameObject gameobject)
	{
		auto* emitter = m_scripted_particle_emitters[gameobject];
		m_project.onComponentDestroyed(emitter->m_gameobject, SCRIPTED_PARTICLE_EMITTER_TYPE, this);
		m_scripted_particle_emitters.erase(emitter->m_gameobject);
		MALMY_DELETE(m_allocator, emitter);
	}


	void cleanup(ParticleEmitter* emitter)
	{
		if (emitter->m_is_valid) return;
		if (!emitter->m_modules.empty()) return;

		m_particle_emitters.erase(emitter->m_gameobject);
		MALMY_DELETE(m_allocator, emitter);
	}


	void destroyParticleEmitterAlpha(GameObject gameobject)
	{
		auto* emitter = m_particle_emitters[gameobject];
		auto* module = emitter->getModule(PARTICLE_EMITTER_ALPHA_TYPE);
		ASSERT(module);

		MALMY_DELETE(m_allocator, module);
		emitter->m_modules.eraseItem(module);
		m_project.onComponentDestroyed(emitter->m_gameobject, PARTICLE_EMITTER_ALPHA_TYPE, this);
		cleanup(emitter);
	}


	void destroyParticleEmitterForce(GameObject gameobject)
	{
		auto* emitter = m_particle_emitters[gameobject];
		auto* module = emitter->getModule(PARTICLE_EMITTER_FORCE_HASH);

		ASSERT(module);

		MALMY_DELETE(m_allocator, module);
		emitter->m_modules.eraseItem(module);
		m_project.onComponentDestroyed(emitter->m_gameobject, PARTICLE_EMITTER_FORCE_HASH, this);
		cleanup(emitter);
	}


	void destroyParticleEmitterSubimage(GameObject gameobject)
	{
		auto* emitter = m_particle_emitters[gameobject];
		auto* module = emitter->getModule(PARTICLE_EMITTER_SUBIMAGE_TYPE);

		ASSERT(module);

		MALMY_DELETE(m_allocator, module);
		emitter->m_modules.eraseItem(module);
		emitter->m_subimage_module = nullptr;
		m_project.onComponentDestroyed(emitter->m_gameobject, PARTICLE_EMITTER_SUBIMAGE_TYPE, this);
		cleanup(emitter);
	}


	void destroyParticleEmitterAttractor(GameObject gameobject)
	{
		auto* emitter = m_particle_emitters[gameobject];
		auto* module = emitter->getModule(PARTICLE_EMITTER_ATTRACTOR_TYPE);

		ASSERT(module);

		MALMY_DELETE(m_allocator, module);
		emitter->m_modules.eraseItem(module);
		m_project.onComponentDestroyed(emitter->m_gameobject, PARTICLE_EMITTER_ATTRACTOR_TYPE, this);
		cleanup(emitter);
	}


	void destroyParticleEmitterSize(GameObject gameobject)
	{
		auto* emitter = m_particle_emitters[gameobject];
		auto* module = emitter->getModule(PARTICLE_EMITTER_SIZE_TYPE);

		ASSERT(module);

		MALMY_DELETE(m_allocator, module);
		emitter->m_modules.eraseItem(module);
		m_project.onComponentDestroyed(emitter->m_gameobject, PARTICLE_EMITTER_SIZE_TYPE, this);
		cleanup(emitter);

	}


	float getParticleEmitterPlaneBounce(GameObject gameobject) override
	{
		auto* emitter = m_particle_emitters[gameobject];
		for(auto* module : emitter->m_modules)
		{
			if(module->getType() == ParticleEmitter::PlaneModule::s_type)
			{
				return static_cast<ParticleEmitter::PlaneModule*>(module)->m_bounce;
			}
		}
		return 0;
	}


	void setParticleEmitterPlaneBounce(GameObject gameobject, float value) override
	{
		auto* emitter = m_particle_emitters[gameobject];
		for (auto* module : emitter->m_modules)
		{
			if (module->getType() == ParticleEmitter::PlaneModule::s_type)
			{
				static_cast<ParticleEmitter::PlaneModule*>(module)->m_bounce = value;
				break;
			}
		}
	}


	float getParticleEmitterAttractorForce(GameObject gameobject) override
	{
		auto* emitter = m_particle_emitters[gameobject];
		for (auto* module : emitter->m_modules)
		{
			if (module->getType() == ParticleEmitter::AttractorModule::s_type)
			{
				return static_cast<ParticleEmitter::AttractorModule*>(module)->m_force;
			}
		}
		return 0;
	}


	void setParticleEmitterAttractorForce(GameObject gameobject, float value) override
	{
		auto* emitter = m_particle_emitters[gameobject];
		for (auto* module : emitter->m_modules)
		{
			if (module->getType() == ParticleEmitter::AttractorModule::s_type)
			{
				static_cast<ParticleEmitter::AttractorModule*>(module)->m_force = value;
				break;
			}
		}
	}


	void destroyParticleEmitterPlane(GameObject gameobject)
	{
		auto* emitter = m_particle_emitters[gameobject];
		auto* module = emitter->getModule(PARTICLE_EMITTER_PLANE_TYPE);

		ASSERT(module);

		MALMY_DELETE(m_allocator, module);
		emitter->m_modules.eraseItem(module);
		m_project.onComponentDestroyed(emitter->m_gameobject, PARTICLE_EMITTER_PLANE_TYPE, this);
		cleanup(emitter);
	}


	void destroyParticleEmitterLinearMovement(GameObject gameobject)
	{
		auto* emitter = m_particle_emitters[gameobject];
		auto* module = emitter->getModule(PARTICLE_EMITTER_LINEAR_MOVEMENT_TYPE);

		ASSERT(module);

		MALMY_DELETE(m_allocator, module);
		emitter->m_modules.eraseItem(module);
		m_project.onComponentDestroyed(emitter->m_gameobject, PARTICLE_EMITTER_LINEAR_MOVEMENT_TYPE, this);
		cleanup(emitter);
	}


	void destroyParticleEmitterSpawnShape(GameObject gameobject)
	{
		auto* emitter = m_particle_emitters[gameobject];
		auto* module = emitter->getModule(PARTICLE_EMITTER_SPAWN_SHAPE_TYPE);

		ASSERT(module);

		MALMY_DELETE(m_allocator, module);
		emitter->m_modules.eraseItem(module);
		m_project.onComponentDestroyed(emitter->m_gameobject, PARTICLE_EMITTER_SPAWN_SHAPE_TYPE, this);
		cleanup(emitter);

	}


	void destroyParticleEmitterRandomRotation(GameObject gameobject)
	{
		auto* emitter = m_particle_emitters[gameobject];
		auto* module = emitter->getModule(PARTICLE_EMITTER_RANDOM_ROTATION_TYPE);

		ASSERT(module);

		MALMY_DELETE(m_allocator, module);
		emitter->m_modules.eraseItem(module);
		m_project.onComponentDestroyed(emitter->m_gameobject, PARTICLE_EMITTER_RANDOM_ROTATION_TYPE, this);
		cleanup(emitter);

	}


	void setParticleEmitterAlpha(GameObject gameobject, const Vec2* values, int count) override
	{
		ASSERT(count > 0);
		ASSERT(values[1].x < 0.001f);
		ASSERT(values[count - 2].x > 0.999f);

		auto* alpha_module = getEmitterModule<ParticleEmitter::AlphaModule>(gameobject);
		if (!alpha_module) return;

		alpha_module->m_values.resize(count);
		for (int i = 0; i < count; ++i)
		{
			alpha_module->m_values[i] = values[i];
		}
		alpha_module->sample();
	}


	void setParticleEmitterSubimageRows(GameObject gameobject, const int& value) override
	{
		auto* module = getEmitterModule<ParticleEmitter::SubimageModule>(gameobject);
		if (module) module->rows = value;
	}


	void setParticleEmitterSubimageCols(GameObject gameobject, const int& value) override
	{
		auto* module = getEmitterModule<ParticleEmitter::SubimageModule>(gameobject);
		if (module) module->cols = value;
	}


	int getParticleEmitterSubimageRows(GameObject gameobject) override
	{
		auto* module = getEmitterModule<ParticleEmitter::SubimageModule>(gameobject);
		return module ? module->rows : 1;
	}


	int getParticleEmitterSubimageCols(GameObject gameobject) override
	{
		auto* module = getEmitterModule<ParticleEmitter::SubimageModule>(gameobject);
		return module ? module->cols : 1;
	}


	void setParticleEmitterAcceleration(GameObject gameobject, const Vec3& value) override
	{
		auto* module = getEmitterModule<ParticleEmitter::ForceModule>(gameobject);
		if (module) module->m_acceleration = value;
	}


	void setParticleEmitterAutoemit(GameObject gameobject, bool autoemit) override
	{
		m_particle_emitters[gameobject]->m_autoemit = autoemit;
	}


	bool getParticleEmitterAutoemit(GameObject gameobject) override
	{
		return m_particle_emitters[gameobject]->m_autoemit;
	}


	void setParticleEmitterLocalSpace(GameObject gameobject, bool local_space) override
	{
		m_particle_emitters[gameobject]->m_local_space = local_space;
	}


	bool getParticleEmitterLocalSpace(GameObject gameobject) override
	{
		return m_particle_emitters[gameobject]->m_local_space;
	}


	Vec3 getParticleEmitterAcceleration(GameObject gameobject) override
	{
		auto* module = getEmitterModule<ParticleEmitter::ForceModule>(gameobject);
		return module ? module->m_acceleration : Vec3();
	}


	int getParticleEmitterSizeCount(GameObject gameobject) override
	{
		auto* module = getEmitterModule<ParticleEmitter::SizeModule>(gameobject);
		return module ? module->m_values.size() : 0; 
	}


	const Vec2* getParticleEmitterSize(GameObject gameobject) override
	{
		auto* module = getEmitterModule<ParticleEmitter::SizeModule>(gameobject);
		return module ? &module->m_values[0] : nullptr;
	}


	void setParticleEmitterSize(GameObject gameobject, const Vec2* values, int count) override
	{
		ASSERT(count > 0);
		ASSERT(values[0].x < 0.001f);
		ASSERT(values[count-1].x > 0.999f);

		auto* module = getEmitterModule<ParticleEmitter::SizeModule>(gameobject);
		if (!module) return;

		auto size_module = static_cast<ParticleEmitter::SizeModule*>(module);
		size_module->m_values.resize(count);
		for (int i = 0; i < count; ++i)
		{
			size_module->m_values[i] = values[i];
		}
		size_module->sample();
	}


	template <typename T>
	T* getEmitterModule(GameObject gameobject) const
	{
		auto& modules = m_particle_emitters[gameobject]->m_modules;
		for (auto* module : modules)
		{
			if (module->getType() == T::s_type)
			{
				return static_cast<T*>(module);
			}
		}
		return nullptr;
	}


	int getParticleEmitterAlphaCount(GameObject gameobject) override 
	{
		auto* module = getEmitterModule<ParticleEmitter::AlphaModule>(gameobject);
		return module ? module->m_values.size() : 0;
	}


	const Vec2* getParticleEmitterAlpha(GameObject gameobject) override
	{
		auto* module = getEmitterModule<ParticleEmitter::AlphaModule>(gameobject);
		return module ? &module->m_values[0] : 0;
	}


	Vec2 getParticleEmitterLinearMovementX(GameObject gameobject) override
	{
		auto* module = getEmitterModule<ParticleEmitter::LinearMovementModule>(gameobject);
		return module ? Vec2(module->m_x.from, module->m_x.to) : Vec2(0, 0);
	}


	void setParticleEmitterLinearMovementX(GameObject gameobject, const Vec2& value) override
	{
		auto* module = getEmitterModule<ParticleEmitter::LinearMovementModule>(gameobject);
		if (module)
		{
			module->m_x = value;
			module->m_x.check();
		}
	}


	Vec2 getParticleEmitterLinearMovementY(GameObject gameobject) override
	{
		auto* module = getEmitterModule<ParticleEmitter::LinearMovementModule>(gameobject);
		return module ? Vec2(module->m_y.from, module->m_y.to) : Vec2(0, 0);
	}


	void setParticleEmitterLinearMovementY(GameObject gameobject, const Vec2& value) override
	{
		auto* module = getEmitterModule<ParticleEmitter::LinearMovementModule>(gameobject);
		if (module)
		{
			module->m_y = value;
			module->m_y.check();
		}
	}


	Vec2 getParticleEmitterLinearMovementZ(GameObject gameobject) override
	{
		auto* module = getEmitterModule<ParticleEmitter::LinearMovementModule>(gameobject);
		return module ? Vec2(module->m_z.from, module->m_z.to) : Vec2(0, 0);
	}


	void setParticleEmitterLinearMovementZ(GameObject gameobject, const Vec2& value) override
	{
		auto* module = getEmitterModule<ParticleEmitter::LinearMovementModule>(gameobject);
		if (module)
		{
			module->m_z = value;
			module->m_z.check();
		}
	}


	Vec2 getParticleEmitterInitialLife(GameObject gameobject) override
	{
		return m_particle_emitters[gameobject]->m_initial_life;
	}


	Vec2 getParticleEmitterSpawnPeriod(GameObject gameobject) override
	{
		return m_particle_emitters[gameobject]->m_spawn_period;
	}


	void setParticleEmitterInitialLife(GameObject gameobject, const Vec2& value) override
	{
		m_particle_emitters[gameobject]->m_initial_life = value;
		m_particle_emitters[gameobject]->m_initial_life.checkZero();
	}


	void setParticleEmitterInitialSize(GameObject gameobject, const Vec2& value) override
	{
		m_particle_emitters[gameobject]->m_initial_size = value;
		m_particle_emitters[gameobject]->m_initial_size.checkZero();
	}


	Vec2 getParticleEmitterInitialSize(GameObject gameobject) override
	{
		return m_particle_emitters[gameobject]->m_initial_size;
	}


	void setParticleEmitterSpawnPeriod(GameObject gameobject, const Vec2& value) override
	{
		auto* emitter = m_particle_emitters[gameobject];
		emitter->m_spawn_period = value;
		emitter->m_spawn_period.from = Math::maximum(0.01f, emitter->m_spawn_period.from);
		emitter->m_spawn_period.checkZero();
	}


	void createTextMesh(GameObject gameobject)
	{
		TextMesh* text = MALMY_NEW(m_allocator, TextMesh)(m_allocator);
		m_text_meshes.insert(gameobject, text);
		m_project.onComponentCreated(gameobject, TEXT_MESH_TYPE, this);
	}


	void createCamera(GameObject gameobject)
	{
		Camera camera;
		camera.is_ortho = false;
		camera.ortho_size = 10;
		camera.gameobject = gameobject;
		camera.fov = Math::degreesToRadians(60);
		camera.screen_width = 800;
		camera.screen_height = 600;
		camera.aspect = 800.0f / 600.0f;
		camera.near = 0.1f;
		camera.far = 10000.0f;
		camera.slot[0] = '\0';
		if (!getCameraInSlot("main").isValid()) copyString(camera.slot, "main");
		m_cameras.insert(gameobject, camera);
		m_project.onComponentCreated(gameobject, CAMERA_TYPE, this);
	}


	void createTerrain(GameObject gameobject)
	{
		Terrain* terrain = MALMY_NEW(m_allocator, Terrain)(m_renderer, gameobject, *this, m_allocator);
		m_terrains.insert(gameobject, terrain);
		m_project.onComponentCreated(gameobject, TERRAIN_TYPE, this);
	}


	void createParticleEmitterRandomRotation(GameObject gameobject)
	{
		int index = allocateParticleEmitter(gameobject);
		auto* emitter = m_particle_emitters.at(index);
		auto module = MALMY_NEW(m_allocator, ParticleEmitter::RandomRotationModule)(*emitter);
		emitter->addModule(module);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_RANDOM_ROTATION_TYPE, this);
	}


	void createParticleEmitterPlane(GameObject gameobject)
	{
		int index = allocateParticleEmitter(gameobject);
		auto* emitter = m_particle_emitters.at(index);
		auto module = MALMY_NEW(m_allocator, ParticleEmitter::PlaneModule)(*emitter);
		emitter->addModule(module);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_PLANE_TYPE, this);
	}


	void createParticleEmitterLinearMovement(GameObject gameobject)
	{
		int index = allocateParticleEmitter(gameobject);
		auto* emitter = m_particle_emitters.at(index);
		auto module = MALMY_NEW(m_allocator, ParticleEmitter::LinearMovementModule)(*emitter);
		emitter->addModule(module);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_LINEAR_MOVEMENT_TYPE, this);
	}


	void createParticleEmitterSpawnShape(GameObject gameobject)
	{
		int index = allocateParticleEmitter(gameobject);
		auto* emitter = m_particle_emitters.at(index);
		auto module = MALMY_NEW(m_allocator, ParticleEmitter::SpawnShapeModule)(*emitter);
		emitter->addModule(module);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_SPAWN_SHAPE_TYPE, this);
	}


	void createParticleEmitterAlpha(GameObject gameobject)
	{
		int index = allocateParticleEmitter(gameobject);
		auto* emitter = m_particle_emitters.at(index);
		auto module = MALMY_NEW(m_allocator, ParticleEmitter::AlphaModule)(*emitter);
		emitter->addModule(module);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_ALPHA_TYPE, this);
	}


	void createParticleEmitterForce(GameObject gameobject)
	{
		int index = allocateParticleEmitter(gameobject);
		auto* emitter = m_particle_emitters.at(index);
		auto module = MALMY_NEW(m_allocator, ParticleEmitter::ForceModule)(*emitter);
		emitter->addModule(module);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_FORCE_HASH, this);
	}


	void createParticleEmitterSubimage(GameObject gameobject)
	{
		int index = allocateParticleEmitter(gameobject);
		auto* emitter = m_particle_emitters.at(index);
		auto module = MALMY_NEW(m_allocator, ParticleEmitter::SubimageModule)(*emitter);
		emitter->addModule(module);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_SUBIMAGE_TYPE, this);
	}


	void createParticleEmitterAttractor(GameObject gameobject)
	{
		int index = allocateParticleEmitter(gameobject);
		auto* emitter = m_particle_emitters.at(index);
		auto module = MALMY_NEW(m_allocator, ParticleEmitter::AttractorModule)(*emitter);
		emitter->addModule(module);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_ATTRACTOR_TYPE, this);
	}


	void createParticleEmitterSize(GameObject gameobject)
	{
		int index = allocateParticleEmitter(gameobject);
		auto* emitter = m_particle_emitters.at(index);
		auto module = MALMY_NEW(m_allocator, ParticleEmitter::SizeModule)(*emitter);
		emitter->addModule(module);
		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_SIZE_TYPE, this);
	}


	void createScriptedParticleEmitter(GameObject gameobject)
	{
		m_scripted_particle_emitters.insert(gameobject, MALMY_NEW(m_allocator, ScriptedParticleEmitter)(gameobject, m_allocator));
		m_project.onComponentCreated(gameobject, SCRIPTED_PARTICLE_EMITTER_TYPE, this);
	}


	void createParticleEmitter(GameObject gameobject)
	{
		int index = allocateParticleEmitter(gameobject);
		m_particle_emitters.at(index)->init();

		m_project.onComponentCreated(gameobject, PARTICLE_EMITTER_TYPE, this);
	}


	int allocateParticleEmitter(GameObject gameobject)
	{
		int index = m_particle_emitters.find(gameobject);
		if (index >= 0) return index;
		return m_particle_emitters.insert(gameobject, MALMY_NEW(m_allocator, ParticleEmitter)(gameobject, m_project, m_allocator));
	}


	ModelInstance* getModelInstances() override
	{
		return m_model_instances.empty() ? nullptr : &m_model_instances[0];
	}


	ModelInstance* getModelInstance(GameObject gameobject) override
	{
		return &m_model_instances[gameobject.index];
	}


	Vec3 getPoseBonePosition(GameObject model_instance, int bone_index)
	{
		Pose* pose = m_model_instances[model_instance.index].pose;
		return pose->positions[bone_index];
	}


	Frustum getPointLightFrustum(int light_idx) const
	{
		const PointLight& light = m_point_lights[light_idx];
		Frustum frustum;
		frustum.computeOrtho(m_project.getPosition(light.m_gameobject),
			Vec3(1, 0, 0),
			Vec3(0, 1, 0),
			light.m_range,
			light.m_range,
			-light.m_range,
			light.m_range);

		return frustum;
	}


	void onGameObjectDestroyed(GameObject gameobject)
	{
		for (auto& i : m_bone_attachments)
		{
			if (i.parent_gameobject == gameobject)
			{
				i.parent_gameobject = INVALID_GAMEOBJECT;
				break;
			}
		}
	}


	void onGameObjectMoved(GameObject gameobject)
	{
		int index = gameobject.index;

		if (index < m_model_instances.size() && m_model_instances[index].gameobject.isValid() &&
			m_model_instances[index].model && m_model_instances[index].model->isReady())
		{
			ModelInstance& r = m_model_instances[index];
			r.matrix = m_project.getMatrix(gameobject);
			if (r.model && r.model->isReady())
			{
				float radius = m_project.getScale(gameobject) * r.model->getBoundingRadius();
				Vec3 position = m_project.getPosition(gameobject);
				m_culling_system->updateBoundingSphere({position, radius}, gameobject);
			}

			float bounding_radius = r.model ? r.model->getBoundingRadius() : 1;
			for (int light_idx = 0, c = m_point_lights.size(); light_idx < c; ++light_idx)
			{
				for (int j = 0, c2 = m_light_influenced_geometry[light_idx].size(); j < c2; ++j)
				{
					if(m_light_influenced_geometry[light_idx][j] == gameobject)
					{
						m_light_influenced_geometry[light_idx].eraseFast(j);
						break;
					}
				}

				Vec3 pos = m_project.getPosition(r.gameobject);
				Frustum frustum = getPointLightFrustum(light_idx);
				if(frustum.isSphereInside(pos, bounding_radius))
				{
					m_light_influenced_geometry[light_idx].push(gameobject);
				}
			}
		}

		int decal_idx = m_decals.find(gameobject);
		if (decal_idx >= 0)
		{
			updateDecalInfo(m_decals.at(decal_idx));
		}

		for (int i = 0, c = m_point_lights.size(); i < c; ++i)
		{
			if (m_point_lights[i].m_gameobject == gameobject)
			{
				detectLightInfluencedGeometry({ m_point_lights[i].m_gameobject.index });
				break;
			}
		}

		bool was_updating = m_is_updating_attachments;
		m_is_updating_attachments = true;
		for (auto& attachment : m_bone_attachments)
		{
			if (attachment.parent_gameobject == gameobject)
			{
				updateBoneAttachment(attachment);
			}
		}
		m_is_updating_attachments = was_updating;

		if (m_is_updating_attachments || m_is_game_running) return;
		for (auto& attachment : m_bone_attachments)
		{
			if (attachment.gameobject == gameobject)
			{
				updateRelativeMatrix(attachment);
				break;
			}
		}

	}

	Engine& getEngine() const override { return m_engine; }


	GameObject getTerrainGameObject(GameObject gameobject) override
	{
		return gameobject;
	}


	Vec2 getTerrainResolution(GameObject gameobject) override
	{
		auto* terrain = m_terrains[gameobject];
		return Vec2((float)terrain->getWidth(), (float)terrain->getHeight());
	}


	GameObject getFirstTerrain() override
	{
		if (m_terrains.empty()) return INVALID_GAMEOBJECT;
		auto iter = m_terrains.begin();
		return iter.value()->getGameObject();
	}


	GameObject getNextTerrain(GameObject gameobject) override
	{
		auto iter = m_terrains.find(gameobject);
		++iter;
		if (!iter.isValid()) return INVALID_GAMEOBJECT;
		return iter.value()->getGameObject();
	}


	Vec3 getTerrainNormalAt(GameObject gameobject, float x, float z) override
	{
		return m_terrains[gameobject]->getNormal(x, z);
	}


	float getTerrainHeightAt(GameObject gameobject, float x, float z) override
	{
		return m_terrains[gameobject]->getHeight(x, z);
	}


	AABB getTerrainAABB(GameObject gameobject) override
	{
		return m_terrains[gameobject]->getAABB();
	}


	Vec2 getTerrainSize(GameObject gameobject) override
	{
		return m_terrains[gameobject]->getSize();
	}


	void setTerrainMaterialPath(GameObject gameobject, const Path& path) override
	{
		if (path.isValid())
		{
			Material* material = static_cast<Material*>(m_engine.getResourceManager().get(Material::TYPE)->load(path));
			m_terrains[gameobject]->setMaterial(material);
		}
		else
		{
			m_terrains[gameobject]->setMaterial(nullptr);
		}
	}


	Material* getTerrainMaterial(GameObject gameobject) override { return m_terrains[gameobject]->getMaterial(); }


	void setDecalScale(GameObject gameobject, const Vec3& value) override
	{
		Decal& decal = m_decals[gameobject];
		decal.scale = value;
		updateDecalInfo(decal);
	}


	Vec3 getDecalScale(GameObject gameobject) override
	{
		return m_decals[gameobject].scale;
	}


	void getDecals(const Frustum& frustum, Array<DecalInfo>& decals) override
	{
		decals.reserve(m_decals.size());
		for (const Decal& decal : m_decals)
		{
			if (!decal.material || !decal.material->isReady()) continue;
			if (frustum.isSphereInside(decal.position, decal.radius)) decals.push(decal);
		}
	}


	void setDecalMaterialPath(GameObject gameobject, const Path& path) override
	{
		ResourceManagerBase* material_manager = m_engine.getResourceManager().get(Material::TYPE);
		Decal& decal = m_decals[gameobject];
		if (decal.material)
		{
			material_manager->unload(*decal.material);
		}
		if (path.isValid())
		{
			decal.material = static_cast<Material*>(material_manager->load(path));
		}
		else
		{
			decal.material = nullptr;
		}
	}


	Path getDecalMaterialPath(GameObject gameobject) override
	{
		Decal& decal = m_decals[gameobject];
		return decal.material ? decal.material->getPath() : Path("");
	}


	Path getTerrainMaterialPath(GameObject gameobject) override
	{
		Terrain* terrain = m_terrains[gameobject];
		if (terrain->getMaterial())
		{
			return terrain->getMaterial()->getPath();
		}
		else
		{
			return Path("");
		}
	}


	void setTerrainXZScale(GameObject gameobject, float scale) override
	{
		m_terrains[gameobject]->setXZScale(scale);
	}

	float getTerrainXZScale(GameObject gameobject) override { return m_terrains[gameobject]->getXZScale(); }


	void setTerrainYScale(GameObject gameobject, float scale) override
	{
		m_terrains[gameobject]->setYScale(scale);
	}

	float getTerrainYScale(GameObject gameobject) override { return m_terrains[gameobject]->getYScale(); }


	Pose* lockPose(GameObject gameobject) override { return m_model_instances[gameobject.index].pose; }
	void unlockPose(GameObject gameobject, bool changed) override
	{
		if (!changed) return;
		if (gameobject.index < m_model_instances.size()
			&& (m_model_instances[gameobject.index].flags.isSet(ModelInstance::IS_BONE_ATTACHMENT_PARENT)) == 0)
		{
			return;
		}

		GameObject parent = gameobject;
		for (BoneAttachment& ba : m_bone_attachments)
		{
			if (ba.parent_gameobject != parent) continue;
			m_is_updating_attachments = true;
			updateBoneAttachment(ba);
			m_is_updating_attachments = false;
		}
	}


	Model* getModelInstanceModel(GameObject gameobject) override { return m_model_instances[gameobject.index].model; }


	static u64 getLayerMask(ModelInstance& model_instance)
	{
		Model* model = model_instance.model;
		if (!model->isReady()) return 1;
		u64 layer_mask = 0;
		for(int i = 0; i < model->getMeshCount(); ++i)
		{ 
			layer_mask |= model->getMesh(i).material->getRenderLayerMask();
		}
		return layer_mask;
	}


	bool isModelInstanceEnabled(GameObject gameobject) override
	{
		ModelInstance& model_instance = m_model_instances[gameobject.index];
		return model_instance.flags.isSet(ModelInstance::ENABLED);
	}


	void enableModelInstance(GameObject gameobject, bool enable) override
	{
		ModelInstance& model_instance = m_model_instances[gameobject.index];
		model_instance.flags.set(ModelInstance::ENABLED, enable);
		if (enable)
		{
			if (!model_instance.model || !model_instance.model->isReady()) return;

			Sphere sphere(m_project.getPosition(model_instance.gameobject), model_instance.model->getBoundingRadius());
			u64 layer_mask = getLayerMask(model_instance);
			if (!m_culling_system->isAdded(gameobject)) m_culling_system->addStatic(gameobject, sphere, layer_mask);
		}
		else
		{
			m_culling_system->removeStatic(gameobject);
		}
	}


	Path getModelInstancePath(GameObject gameobject) override
	{
		return m_model_instances[gameobject.index].model ? m_model_instances[gameobject.index].model->getPath() : Path("");
	}


	int getModelInstanceMaterialsCount(GameObject gameobject) override
	{
		return m_model_instances[gameobject.index].model ? m_model_instances[gameobject.index].mesh_count : 0;
	}


	void setModelInstancePath(GameObject gameobject, const Path& path) override
	{
		ModelInstance& r = m_model_instances[gameobject.index];

		auto* manager = m_engine.getResourceManager().get(Model::TYPE);
		if (path.isValid())
		{
			Model* model = static_cast<Model*>(manager->load(path));
			setModel(gameobject, model);
		}
		else
		{
			setModel(gameobject, nullptr);
		}
		r.matrix = m_project.getMatrix(r.gameobject);
	}


	void forceGrassUpdate(GameObject gameobject) override { m_terrains[gameobject]->forceGrassUpdate(); }


	void getTerrainInfos(const Frustum& frustum, const Vec3& lod_ref_point, Array<TerrainInfo>& infos) override
	{
		PROFILE_FUNCTION();
		infos.reserve(m_terrains.size());
		for (auto* terrain : m_terrains)
		{
			terrain->getInfos(infos, frustum, lod_ref_point);
		}
	}


	void getGrassInfos(const Frustum& frustum, GameObject camera, Array<GrassInfo>& infos) override
	{
		PROFILE_FUNCTION();

		if (!m_is_grass_enabled) return;

		for (auto* terrain : m_terrains)
		{
			terrain->getGrassInfos(frustum, infos, camera);
		}
	}


	static int LUA_castCameraRay(lua_State* L)
	{
		auto* scene = LuaWrapper::checkArg<RenderSceneImpl*>(L, 1);
		const char* slot = LuaWrapper::checkArg<const char*>(L, 2);
		float x, y;
		GameObject camera_gameobject = scene->getCameraInSlot(slot);
		if (!camera_gameobject.isValid()) return 0;
		if (lua_gettop(L) > 3)
		{
			x = LuaWrapper::checkArg<float>(L, 3);
			y = LuaWrapper::checkArg<float>(L, 4);
		}
		else
		{
			x = scene->getCameraScreenWidth(camera_gameobject) * 0.5f;
			y = scene->getCameraScreenHeight(camera_gameobject) * 0.5f;
		}

		Vec3 origin, dir;
		scene->getRay(camera_gameobject, {x, y}, origin, dir);

		RayCastModelHit hit = scene->castRay(origin, dir, INVALID_GAMEOBJECT);
		LuaWrapper::push(L, hit.m_is_hit);
		LuaWrapper::push(L, hit.m_is_hit ? hit.m_origin + hit.m_dir * hit.m_t : Vec3(0, 0, 0));

		return 2;
	}


	static bgfx::TextureHandle* LUA_getTextureHandle(RenderScene* scene, int resource_idx)
	{
		Resource* res = scene->getEngine().getLuaResource(resource_idx);
		if (!res) return nullptr;
		return &static_cast<Texture*>(res)->handle;
	}

	
	static void LUA_setTexturePixel(Texture* texture, int x, int y, u32 value)
	{
		if (!texture) return;
		if (!texture->isReady()) return;
		if (texture->data.empty()) return;
		if (texture->bytes_per_pixel != 4) return;

		x = Math::clamp(x, 0, texture->width - 1);
		y = Math::clamp(y, 0, texture->height - 1);

		((u32*)&texture->data[0])[x + y * texture->width] = value;
	}


	static void LUA_updateTextureData(Texture* texture, int x, int y, int w, int h)
	{
		if (!texture) return;
		if (!texture->isReady()) return;
		if (texture->data.empty()) return;

		texture->onDataUpdated(x, y, w, h);
	}

	
	static int LUA_getTextureWidth(Texture* texture)
	{
		if (!texture) return 0;
		if (!texture->isReady()) return 0;

		return texture->width;
	}


	static int LUA_getTextureHeight(Texture* texture)
	{
		if (!texture) return 0;
		if (!texture->isReady()) return 0;

		return texture->height;
	}


	static float LUA_getTerrainHeightAt(RenderSceneImpl* render_scene, GameObject gameobject, int x, int z)
	{
		return render_scene->m_terrains[gameobject]->getHeight(x, z);
	}


	static void LUA_emitParticle(RenderSceneImpl* render_scene, GameObject emitter)
	{
		int idx = render_scene->m_particle_emitters.find(emitter);
		if (idx < 0) return;
		return render_scene->m_particle_emitters.at(idx)->emit();
	}


	void setTerrainHeightAt(GameObject gameobject, int x, int z, float height)
	{
		m_terrains[gameobject]->setHeight(x, z, height);
	}

	static u32 LUA_getTexturePixel(Texture* texture, int x, int y)
	{
		if (!texture) return 0;
		if (!texture->isReady()) return 0;
		if (texture->data.empty()) return 0;
		if (texture->bytes_per_pixel != 4) return 0;
		
		x = Math::clamp(x, 0, texture->width - 1);
		y = Math::clamp(y, 0, texture->height - 1);

		return ((u32*)&texture->data[0])[x + y * texture->width];
	}


	static Pipeline* LUA_createPipeline(Engine* engine, const char* path)
	{
		Renderer& renderer = *static_cast<Renderer*>(engine->getPluginManager().getPlugin("renderer"));
		Pipeline* pipeline = Pipeline::create(renderer, Path(path), "", renderer.getEngine().getAllocator());
		pipeline->load();
		return pipeline;
	}


	static void LUA_destroyPipeline(Pipeline* pipeline)
	{
		Pipeline::destroy(pipeline);
	}


	static void LUA_setPipelineScene(Pipeline* pipeline, RenderScene* scene)
	{
		pipeline->setScene(scene);
	}


	static RenderScene* LUA_getPipelineScene(Pipeline* pipeline)
	{
		return pipeline->getScene();
	}


	static void LUA_pipelineRender(Pipeline* pipeline, int w, int h)
	{
		pipeline->resize(w, h);
		pipeline->render();
	}


	static bgfx::TextureHandle* LUA_getRenderBuffer(Pipeline* pipeline,
		const char* framebuffer_name,
		int renderbuffer_idx)
	{
		bgfx::TextureHandle& handle = pipeline->getRenderbuffer(framebuffer_name, renderbuffer_idx);
		return &handle;
	}


	static Texture* LUA_getMaterialTexture(Material* material, int texture_index)
	{
		if (!material) return nullptr;
		return material->getTexture(texture_index);
	}


	static void LUA_setModelInstancePath(IScene* scene, int component, const char* path)
	{
		RenderScene* render_scene = (RenderScene*)scene;
		render_scene->setModelInstancePath({component}, Path(path));
	}


	static int LUA_getModelBoneIndex(Model* model, const char* bone)
	{
		if (!model) return 0;
		return model->getBoneIndex(crc32(bone)).value();
	}


	static unsigned int LUA_compareTGA(RenderSceneImpl* scene, const char* path, const char* path_preimage, int min_diff)
	{
		auto& fs = scene->m_engine.getFileSystem();
		auto file1 = fs.open(fs.getDefaultDevice(), Path(path), FS::Mode::OPEN_AND_READ);
		auto file2 = fs.open(fs.getDefaultDevice(), Path(path_preimage), FS::Mode::OPEN_AND_READ);
		if (!file1)
		{
			if (file2) fs.close(*file2);
			g_log_error.log("render_test") << "Failed to open " << path;
			return 0xffffFFFF;
		}
		else if (!file2)
		{
			fs.close(*file1);
			g_log_error.log("render_test") << "Failed to open " << path_preimage;
			return 0xffffFFFF;
		}
		unsigned int result = Texture::compareTGA(file1, file2, min_diff, scene->m_allocator);
		fs.close(*file1);
		fs.close(*file2);
		return result;
	}


	static void LUA_makeScreenshot(RenderSceneImpl* scene, const char* path)
	{
		scene->m_renderer.makeScreenshot(Path(path));
	}


	static void LUA_setModelInstanceMaterial(RenderScene* scene,
		GameObject gameobject,
		int index,
		const char* path)
	{
		scene->setModelInstanceMaterial(gameobject, index, Path(path));
	}


	bool isGrassEnabled() const override
	{
		return m_is_grass_enabled;
	}


	int getGrassRotationMode(GameObject gameobject, int index) override
	{
		return (int)m_terrains[gameobject]->getGrassTypeRotationMode(index);
	}


	void setGrassRotationMode(GameObject gameobject, int index, int value) override
	{
		m_terrains[gameobject]->setGrassTypeRotationMode(index, (Terrain::GrassType::RotationMode)value);
	}


	float getGrassDistance(GameObject gameobject, int index) override
	{
		return m_terrains[gameobject]->getGrassTypeDistance(index);
	}


	void setGrassDistance(GameObject gameobject, int index, float value) override
	{
		m_terrains[gameobject]->setGrassTypeDistance(index, value);
	}


	void enableGrass(bool enabled) override { m_is_grass_enabled = enabled; }


	void setGrassDensity(GameObject gameobject, int index, int density) override
	{
		m_terrains[gameobject]->setGrassTypeDensity(index, density);
	}


	int getGrassDensity(GameObject gameobject, int index) override
	{
		return m_terrains[gameobject]->getGrassTypeDensity(index);
	}


	void setGrassPath(GameObject gameobject, int index, const Path& path) override
	{
		m_terrains[gameobject]->setGrassTypePath(index, path);
	}


	Path getGrassPath(GameObject gameobject, int index) override
	{
		return m_terrains[gameobject]->getGrassTypePath(index);
	}


	int getGrassCount(GameObject gameobject) override
	{
		return m_terrains[gameobject]->getGrassTypeCount();
	}


	void addGrass(GameObject gameobject, int index) override
	{
		m_terrains[gameobject]->addGrassType(index);
	}


	void removeGrass(GameObject gameobject, int index) override
	{
		m_terrains[gameobject]->removeGrassType(index);
	}


	GameObject getFirstModelInstance() override
	{
		return getNextModelInstance(INVALID_GAMEOBJECT);
	}


	GameObject getNextModelInstance(GameObject gameobject) override
	{
		for(int i = gameobject.index + 1; i < m_model_instances.size(); ++i)
		{
			if (m_model_instances[i].gameobject != INVALID_GAMEOBJECT) return {i};
		}
		return INVALID_GAMEOBJECT;
	}

	
	int getClosestPointLights(const Vec3& reference_pos,
		GameObject* lights,
		int max_lights) override
	{

		float dists[16];
		ASSERT(max_lights <= lengthOf(dists));
		ASSERT(max_lights > 0);
		if (m_point_lights.empty()) return 0;

		int light_count = 0;
		for (auto light : m_point_lights)
		{
			Vec3 light_pos = m_project.getPosition(light.m_gameobject);
			float dist_squared = (reference_pos - light_pos).squaredLength();

			dists[light_count] = dist_squared;
			lights[light_count] = { light.m_gameobject.index };

			for (int i = light_count; i > 0 && dists[i - 1] > dists[i]; --i)
			{
				float tmp = dists[i];
				dists[i] = dists[i - 1];
				dists[i - 1] = tmp;

				GameObject tmp2 = lights[i];
				lights[i] = lights[i - 1];
				lights[i - 1] = tmp2;
			}
			++light_count;
			if (light_count == max_lights)
			{
				break;
			}
		}

		for (int i = max_lights; i < m_point_lights.size(); ++i)
		{
			PointLight& light = m_point_lights[i];
			Vec3 light_pos = m_project.getPosition(light.m_gameobject);
			float dist_squared = (reference_pos - light_pos).squaredLength();

			if (dist_squared < dists[max_lights - 1])
			{
				dists[max_lights - 1] = dist_squared;
				lights[max_lights - 1] = { light.m_gameobject.index };

				for (int i = max_lights - 1; i > 0 && dists[i - 1] > dists[i];
					 --i)
				{
					float tmp = dists[i];
					dists[i] = dists[i - 1];
					dists[i - 1] = tmp;

					GameObject tmp2 = lights[i];
					lights[i] = lights[i - 1];
					lights[i - 1] = tmp2;
				}
			}
		}

		return light_count;
	}


	void getPointLights(const Frustum& frustum, Array<GameObject>& lights) override
	{
		for (int i = 0, ci = m_point_lights.size(); i < ci; ++i)
		{
			PointLight& light = m_point_lights[i];

			if (frustum.isSphereInside(m_project.getPosition(light.m_gameobject), light.m_range))
			{
				lights.push(light.m_gameobject);
			}
		}
	}


	void setLightCastShadows(GameObject gameobject, bool cast_shadows) override
	{
		m_point_lights[m_point_lights_map[gameobject]].m_cast_shadows = cast_shadows;
	}


	bool getLightCastShadows(GameObject gameobject) override
	{
		return m_point_lights[m_point_lights_map[gameobject]].m_cast_shadows;
	}


	void getPointLightInfluencedGeometry(GameObject light,
		GameObject camera,
		const Vec3& lod_ref_point,
		const Frustum& frustum,
		Array<MeshInstance>& infos) override
	{
		PROFILE_FUNCTION();

		int light_index = m_point_lights_map[light];
		float lod_multiplier = getCameraLODMultiplier(camera);
		float final_lod_multiplier = m_lod_multiplier * lod_multiplier;
		for (int j = 0, cj = m_light_influenced_geometry[light_index].size(); j < cj; ++j)
		{
			GameObject model_instance_gameobject = m_light_influenced_geometry[light_index][j];
			ModelInstance& model_instance = m_model_instances[model_instance_gameobject.index];
			const Sphere& sphere = m_culling_system->getSphere(model_instance_gameobject);
			float squared_distance = (model_instance.matrix.getTranslation() - lod_ref_point).squaredLength();
			squared_distance *= final_lod_multiplier;

			if (frustum.isSphereInside(sphere.position, sphere.radius))
			{
				LODMeshIndices lod = model_instance.model->getLODMeshIndices(squared_distance);
				for (int k = lod.from, c = lod.to; k <= c; ++k)
				{
					auto& info = infos.emplace();
					info.mesh = &model_instance.model->getMesh(k);
					info.owner = model_instance_gameobject;
				}
			}
		}
	}


	void getPointLightInfluencedGeometry(GameObject light, 
		GameObject camera, 
		const Vec3& lod_ref_point,
		Array<MeshInstance>& infos) override
	{
		PROFILE_FUNCTION();

		int light_index = m_point_lights_map[light];
		auto& geoms = m_light_influenced_geometry[light_index];
		float lod_multiplier = getCameraLODMultiplier(camera);
		float final_lod_multiplier = m_lod_multiplier * lod_multiplier;
		for (int j = 0, cj = geoms.size(); j < cj; ++j)
		{
			const ModelInstance& model_instance = m_model_instances[geoms[j].index];
			float squared_distance = (model_instance.matrix.getTranslation() - lod_ref_point).squaredLength();
			squared_distance *= final_lod_multiplier;
			LODMeshIndices lod = model_instance.model->getLODMeshIndices(squared_distance);
			for (int k = lod.from, kc = lod.to; k <= kc; ++k)
			{
				auto& info = infos.emplace();
				info.mesh = &model_instance.model->getMesh(k);
				info.owner = geoms[j];
			}
		}
	}


	void getModelInstanceEntities(const Frustum& frustum, Array<GameObject>& entities) override
	{
		PROFILE_FUNCTION();

		auto& results = m_culling_system->cull(frustum, ~0ULL);

		for (auto& subresults : results)
		{
			for (GameObject model_instance : subresults)
			{
				entities.push(model_instance);
			}
		}
	}


	float getCameraLODMultiplier(GameObject camera)
	{
		float lod_multiplier;
		if (isCameraOrtho(camera))
		{
			lod_multiplier = 1;
		}
		else
		{
			lod_multiplier = getCameraFOV(camera) / Math::degreesToRadians(60);
			lod_multiplier *= lod_multiplier;
		}
		return lod_multiplier;
	}


	Array<Array<MeshInstance>>& getModelInstanceInfos(const Frustum& frustum,
		const Vec3& lod_ref_point,
		GameObject camera,
		u64 layer_mask) override
	{
		for (auto& i : m_temporary_infos) i.clear();
		const CullingSystem::Results& results = m_culling_system->cull(frustum, layer_mask);

		while (m_temporary_infos.size() < results.size())
		{
			m_temporary_infos.emplace(m_allocator);
		}
		while (m_temporary_infos.size() > results.size())
		{
			m_temporary_infos.pop();
		}

		LambdaStorage<64> jobs[64];
		ASSERT(results.size() <= lengthOf(jobs));

		JobSystem::SignalHandle counter = JobSystem::INVALID_HANDLE;
		for (int subresult_index = 0; subresult_index < results.size(); ++subresult_index) {
			Array<MeshInstance>& subinfos = m_temporary_infos[subresult_index];
			subinfos.clear();

			jobs[subresult_index] = [&layer_mask, &subinfos, this, &results, subresult_index, lod_ref_point, camera]() {
				PROFILE_BLOCK("Temporary Info Job");
				PROFILE_INT("ModelInstance count", results[subresult_index].size());
				if (results[subresult_index].empty()) return;

				float lod_multiplier = getCameraLODMultiplier(camera);
				Vec3 ref_point = lod_ref_point;
				float final_lod_multiplier = m_lod_multiplier * lod_multiplier;
				const GameObject* MALMY_RESTRICT raw_subresults = &results[subresult_index][0];
				ModelInstance* MALMY_RESTRICT model_instances = &m_model_instances[0];
				for (int i = 0, c = results[subresult_index].size(); i < c; ++i)
				{
					const ModelInstance* MALMY_RESTRICT model_instance = &model_instances[raw_subresults[i].index];
					float squared_distance = (model_instance->matrix.getTranslation() - ref_point).squaredLength();
					squared_distance *= final_lod_multiplier;

					const Model* MALMY_RESTRICT model = model_instance->model;
					LODMeshIndices lod = model->getLODMeshIndices(squared_distance);
					for (int j = lod.from, c = lod.to; j <= c; ++j)
					{
						Mesh& mesh = model_instance->meshes[j];
						if ((mesh.layer_mask & layer_mask) == 0) continue;
						
						MeshInstance& info = subinfos.emplace();
						info.owner = raw_subresults[i];
						info.mesh = &mesh;
						info.depth = squared_distance;
					}
				}
				if (!subinfos.empty())
				{
					PROFILE_BLOCK("Sort");
					MeshInstance* begin = &subinfos[0];
					MeshInstance* end = begin + subinfos.size();

					auto cmp = [](const MeshInstance& a, const MeshInstance& b) -> bool {
						if (a.mesh != b.mesh) return a.mesh < b.mesh;
						return (a.depth < b.depth);
					};
					std::sort(begin, end, cmp);
				}
			};
			JobSystem::run(&jobs[subresult_index], [](void* user_ptr) { ((LambdaStorage<64>*)user_ptr)->invoke(); }, &counter, JobSystem::INVALID_HANDLE);
		}
		JobSystem::wait(counter);

		return m_temporary_infos;
	}


	void setCameraSlot(GameObject gameobject, const char* slot) override
	{
		auto& camera = m_cameras[gameobject];
		copyString(camera.slot, lengthOf(camera.slot), slot);
	}


	const char* getCameraSlot(GameObject camera) override { return m_cameras[camera].slot; }
	float getCameraFOV(GameObject camera) override { return m_cameras[camera].fov; }
	void setCameraFOV(GameObject camera, float fov) override { m_cameras[camera].fov = fov; }
	void setCameraNearPlane(GameObject camera, float near_plane) override { m_cameras[camera].near = Math::maximum(near_plane, 0.00001f); }
	float getCameraNearPlane(GameObject camera) override { return m_cameras[camera].near; }
	void setCameraFarPlane(GameObject camera, float far_plane) override { m_cameras[camera].far = far_plane; }
	float getCameraFarPlane(GameObject camera) override { return m_cameras[camera].far; }
	float getCameraScreenWidth(GameObject camera) override { return m_cameras[camera].screen_width; }
	float getCameraScreenHeight(GameObject camera) override { return m_cameras[camera].screen_height; }


	void setGlobalLODMultiplier(float multiplier) { m_lod_multiplier = multiplier; }
	float getGlobalLODMultiplier() const { return m_lod_multiplier; }


	Matrix getCameraViewProjection(GameObject gameobject) override
	{
		Matrix view = m_project.getMatrix(gameobject);
		view.fastInverse();
		return getCameraProjection(gameobject) * view;
	}


	Matrix getCameraProjection(GameObject gameobject) override
	{
		Camera& camera = m_cameras[gameobject];
		Matrix mtx;
		float ratio = camera.screen_height > 0 ? camera.screen_width / camera.screen_height : 1;
		bool is_homogenous_depth = bgfx::getCaps()->homogeneousDepth;
		if (camera.is_ortho)
		{
			mtx.setOrtho(-camera.ortho_size * ratio,
				camera.ortho_size * ratio,
				-camera.ortho_size,
				camera.ortho_size,
				camera.near,
				camera.far,
				is_homogenous_depth,
				true);
		}
		else
		{
			mtx.setPerspective(camera.fov, ratio, camera.near, camera.far, is_homogenous_depth, true);
		}
		return mtx;
	}


	void setCameraScreenSize(GameObject camera, int w, int h) override
	{
		auto& cam = m_cameras[{camera.index}];
		cam.screen_width = (float)w;
		cam.screen_height = (float)h;
		cam.aspect = w / (float)h;
	}


	Vec2 getCameraScreenSize(GameObject camera) override
	{
		auto& cam = m_cameras[{camera.index}];
		return Vec2(cam.screen_width, cam.screen_height);
	}


	float getCameraOrthoSize(GameObject camera) override { return m_cameras[{camera.index}].ortho_size; }
	void setCameraOrthoSize(GameObject camera, float value) override { m_cameras[{camera.index}].ortho_size = value; }
	bool isCameraOrtho(GameObject camera) override { return m_cameras[{camera.index}].is_ortho; }
	void setCameraOrtho(GameObject camera, bool is_ortho) override { m_cameras[{camera.index}].is_ortho = is_ortho; }


	const Array<DebugTriangle>& getDebugTriangles() const override { return m_debug_triangles; }
	const Array<DebugLine>& getDebugLines() const override { return m_debug_lines; }
	const Array<DebugPoint>& getDebugPoints() const override { return m_debug_points; }


	void addDebugSphere(const Vec3& center,
		float radius,
		u32 color,
		float life) override
	{
		static const int COLS = 36;
		static const int ROWS = COLS >> 1;
		static const float STEP = (Math::PI / 180.0f) * 360.0f / COLS;
		int p2 = COLS >> 1;
		int r2 = ROWS >> 1;
		float prev_ci = 1;
		float prev_si = 0;
		for (int y = -r2; y < r2; ++y)
		{
			float cy = cos(y * STEP);
			float cy1 = cos((y + 1) * STEP);
			float sy = sin(y * STEP);
			float sy1 = sin((y + 1) * STEP);

			for (int i = -p2; i < p2; ++i)
			{
				float ci = cos(i * STEP);
				float si = sin(i * STEP);
				addDebugLine(Vec3(center.x + radius * ci * cy,
								  center.y + radius * sy,
								  center.z + radius * si * cy),
							 Vec3(center.x + radius * ci * cy1,
								  center.y + radius * sy1,
								  center.z + radius * si * cy1),
							 color,
							 life);
				addDebugLine(Vec3(center.x + radius * ci * cy,
								  center.y + radius * sy,
								  center.z + radius * si * cy),
							 Vec3(center.x + radius * prev_ci * cy,
								  center.y + radius * sy,
								  center.z + radius * prev_si * cy),
							 color,
							 life);
				addDebugLine(Vec3(center.x + radius * prev_ci * cy1,
								  center.y + radius * sy1,
								  center.z + radius * prev_si * cy1),
							 Vec3(center.x + radius * ci * cy1,
								  center.y + radius * sy1,
								  center.z + radius * si * cy1),
							 color,
							 life);
				prev_ci = ci;
				prev_si = si;
			}
		}
	}


	void addDebugHalfSphere(const Matrix& transform, float radius, bool top, u32 color, float life)
	{
		Vec3 center = transform.getTranslation();
		Vec3 x_vec = transform.getXVector();
		Vec3 y_vec = transform.getYVector();
		if (!top) y_vec *= -1;
		Vec3 z_vec = transform.getZVector();
		static const int COLS = 36;
		static const int ROWS = COLS >> 1;
		static const float STEP = Math::degreesToRadians(360.0f) / COLS;
		for (int y = 0; y < ROWS >> 1; ++y)
		{
			float cy = cos(y * STEP);
			float cy1 = cos((y + 1) * STEP);
			float sy = sin(y * STEP);
			float sy1 = sin((y + 1) * STEP);
			float prev_ci = cos(-STEP);
			float prev_si = sin(-STEP);

			Vec3 y_offset = y_vec * sy;
			Vec3 y_offset1 = y_vec * sy1;

			for (int i = 0; i < COLS; ++i)
			{
				float ci = cos(i * STEP);
				float si = sin(i * STEP);

				addDebugLine(
					center + radius * (x_vec * ci * cy + z_vec * si * cy + y_offset),
					center + radius * (x_vec * prev_ci * cy + z_vec * prev_si * cy + y_offset),
					color,
					life);
				addDebugLine(
					center + radius * (x_vec * ci * cy + z_vec * si * cy + y_offset),
					center + radius * (x_vec * ci * cy1 + z_vec * si * cy1 + y_offset1),
					color,
					life);
				prev_ci = ci;
				prev_si = si;
			}
		}
	}


	void addDebugHalfSphere(const Vec3& center, float radius, bool top, u32 color, float life)
	{
		static const int COLS = 36;
		static const int ROWS = COLS >> 1;
		static const float STEP = (Math::PI / 180.0f) * 360.0f / COLS;
		int p2 = COLS >> 1;
		int yfrom = top ? 0 : -(ROWS >> 1);
		int yto = top ? ROWS >> 1 : 0;
		for (int y = yfrom; y < yto; ++y)
		{
			float cy = cos(y * STEP);
			float cy1 = cos((y + 1) * STEP);
			float sy = sin(y * STEP);
			float sy1 = sin((y + 1) * STEP);
			float prev_ci = cos((-p2 - 1) * STEP);
			float prev_si = sin((-p2 - 1) * STEP);

			for (int i = -p2; i < p2; ++i)
			{
				float ci = cos(i * STEP);
				float si = sin(i * STEP);
				addDebugLine(Vec3(center.x + radius * ci * cy,
					center.y + radius * sy,
					center.z + radius * si * cy),
					Vec3(center.x + radius * ci * cy1,
					center.y + radius * sy1,
					center.z + radius * si * cy1),
					color,
					life);
				addDebugLine(Vec3(center.x + radius * ci * cy,
					center.y + radius * sy,
					center.z + radius * si * cy),
					Vec3(center.x + radius * prev_ci * cy,
					center.y + radius * sy,
					center.z + radius * prev_si * cy),
					color,
					life);
				addDebugLine(Vec3(center.x + radius * prev_ci * cy1,
					center.y + radius * sy1,
					center.z + radius * prev_si * cy1),
					Vec3(center.x + radius * ci * cy1,
					center.y + radius * sy1,
					center.z + radius * si * cy1),
					color,
					life);
				prev_ci = ci;
				prev_si = si;
			}
		}
	}


	void addDebugTriangle(const Vec3& p0,
		const Vec3& p1,
		const Vec3& p2,
		u32 color,
		float life) override
	{
		DebugTriangle& tri = m_debug_triangles.emplace();
		tri.p0 = p0;
		tri.p1 = p1;
		tri.p2 = p2;
		tri.color = ARGBToABGR(color);
		tri.life = life;
	}


	void addDebugCapsule(const Vec3& position,
		float height,
		float radius,
		u32 color,
		float life) override
	{
		addDebugHalfSphere(position + Vec3(0, radius, 0), radius, false, color, life);
		addDebugHalfSphere(position + Vec3(0, radius + height, 0), radius, true, color, life);

		Vec3 z_vec(0, 0, 1.0f);
		Vec3 x_vec(1.0f, 0, 0);
		z_vec.normalize();
		x_vec.normalize();
		Vec3 bottom = position + Vec3(0, radius, 0);
		Vec3 top = bottom + Vec3(0, height, 0);
		for (int i = 1; i <= 32; ++i)
		{
			float a = i / 32.0f * 2 * Math::PI;
			float x = cosf(a) * radius;
			float z = sinf(a) * radius;
			addDebugLine(bottom + x_vec * x + z_vec * z,
				top + x_vec * x + z_vec * z,
				color,
				life);
		}
	}


	void addDebugCapsule(const Matrix& transform,
		float height,
		float radius,
		u32 color,
		float life) override
	{
		Vec3 x_vec = transform.getXVector();
		Vec3 y_vec = transform.getYVector();
		Vec3 z_vec = transform.getZVector();
		Vec3 position = transform.getTranslation();

		Matrix tmp = transform;
		tmp.setTranslation(transform.getTranslation() + y_vec * radius);
		addDebugHalfSphere(tmp, radius, false, color, life);
		tmp.setTranslation(transform.getTranslation() + y_vec * (radius + height));
		addDebugHalfSphere(tmp, radius, true, color, life);

		Vec3 bottom = position + y_vec * radius;
		Vec3 top = bottom + y_vec * height;
		for (int i = 1; i <= 32; ++i)
		{
			float a = i / 32.0f * 2 * Math::PI;
			float x = cosf(a) * radius;
			float z = sinf(a) * radius;
			addDebugLine(bottom + x_vec * x + z_vec * z, top + x_vec * x + z_vec * z, color, life);
		}
	}



	void addDebugCylinder(const Vec3& position,
								  const Vec3& up,
								  float radius,
								  u32 color,
								  float life) override
	{
		Vec3 z_vec(-up.y, up.x, 0);
		Vec3 x_vec = crossProduct(up, z_vec);
		float prevx = radius;
		float prevz = 0;
		z_vec.normalize();
		x_vec.normalize();
		Vec3 top = position + up;
		for (int i = 1; i <= 32; ++i)
		{
			float a = i / 32.0f * 2 * Math::PI;
			float x = cosf(a) * radius;
			float z = sinf(a) * radius;
			addDebugLine(position + x_vec * x + z_vec * z,
						 position + x_vec * prevx + z_vec * prevz,
						 color,
						 life);
			addDebugLine(top + x_vec * x + z_vec * z,
						 top + x_vec * prevx + z_vec * prevz,
						 color,
						 life);
			addDebugLine(position + x_vec * x + z_vec * z,
						 top + x_vec * x + z_vec * z,
						 color,
						 life);
			prevx = x;
			prevz = z;
		}
	}


	void addDebugCube(const Vec3& pos,
		const Vec3& dir,
		const Vec3& up,
		const Vec3& right,
		u32 color,
		float life) override
	{
		addDebugLine(pos + dir + up + right, pos + dir + up - right, color, life);
		addDebugLine(pos - dir + up + right, pos - dir + up - right, color, life);
		addDebugLine(pos + dir + up + right, pos - dir + up + right, color, life);
		addDebugLine(pos + dir + up - right, pos - dir + up - right, color, life);

		addDebugLine(pos + dir - up + right, pos + dir - up - right, color, life);
		addDebugLine(pos - dir - up + right, pos - dir - up - right, color, life);
		addDebugLine(pos + dir - up + right, pos - dir - up + right, color, life);
		addDebugLine(pos + dir - up - right, pos - dir - up - right, color, life);

		addDebugLine(pos + dir + up + right, pos + dir - up + right, color, life);
		addDebugLine(pos + dir + up - right, pos + dir - up - right, color, life);
		addDebugLine(pos - dir + up + right, pos - dir - up + right, color, life);
		addDebugLine(pos - dir + up - right, pos - dir - up - right, color, life);

	}


	void addDebugCubeSolid(const Vec3& min,
		const Vec3& max,
		u32 color,
		float life) override
	{
		Vec3 a = min;
		Vec3 b = min;
		Vec3 c = max;

		b.x = max.x;
		c.z = min.z;
		addDebugTriangle(a, c, b, color, life);
		b.x = min.x;
		b.y = max.y;
		addDebugTriangle(a, b, c, color, life);

		b = max;
		c = max;
		a.z = max.z;
		b.y = min.y;
		addDebugTriangle(a, b, c, color, life);
		b.x = min.x;
		b.y = max.y;
		addDebugTriangle(a, c, b, color, life);

		a = min;
		b = min;
		c = max;

		b.x = max.x;
		c.y = min.y;
		addDebugTriangle(a, b, c, color, life);
		b.x = min.x;
		b.z = max.z;
		addDebugTriangle(a, c, b, color, life);

		b = max;
		c = max;
		a.y = max.y;
		b.z = min.z;
		addDebugTriangle(a, c, b, color, life);
		b.x = min.x;
		b.z = max.z;
		addDebugTriangle(a, b, c, color, life);

		a = min;
		b = min;
		c = max;

		b.y = max.y;
		c.x = min.x;
		addDebugTriangle(a, c, b, color, life);
		b.y = min.y;
		b.z = max.z;
		addDebugTriangle(a, b, c, color, life);

		b = max;
		c = max;
		a.x = max.x;
		b.z = min.z;
		addDebugTriangle(a, b, c, color, life);
		b.y = min.y;
		b.z = max.z;
		addDebugTriangle(a, c, b, color, life);
	}



	void addDebugCube(const Vec3& min,
							  const Vec3& max,
							  u32 color,
							  float life) override
	{
		Vec3 a = min;
		Vec3 b = min;
		b.x = max.x;
		addDebugLine(a, b, color, life);
		a.set(b.x, b.y, max.z);
		addDebugLine(a, b, color, life);
		b.set(min.x, a.y, a.z);
		addDebugLine(a, b, color, life);
		a.set(b.x, b.y, min.z);
		addDebugLine(a, b, color, life);

		a = min;
		a.y = max.y;
		b = a;
		b.x = max.x;
		addDebugLine(a, b, color, life);
		a.set(b.x, b.y, max.z);
		addDebugLine(a, b, color, life);
		b.set(min.x, a.y, a.z);
		addDebugLine(a, b, color, life);
		a.set(b.x, b.y, min.z);
		addDebugLine(a, b, color, life);

		a = min;
		b = a;
		b.y = max.y;
		addDebugLine(a, b, color, life);
		a.x = max.x;
		b.x = max.x;
		addDebugLine(a, b, color, life);
		a.z = max.z;
		b.z = max.z;
		addDebugLine(a, b, color, life);
		a.x = min.x;
		b.x = min.x;
		addDebugLine(a, b, color, life);
	}


	void addDebugFrustum(const Frustum& frustum, u32 color, float life) override
	{
		addDebugLine(frustum.points[0], frustum.points[1], color, life);
		addDebugLine(frustum.points[1], frustum.points[2], color, life);
		addDebugLine(frustum.points[2], frustum.points[3], color, life);
		addDebugLine(frustum.points[3], frustum.points[0], color, life);

		addDebugLine(frustum.points[4], frustum.points[5], color, life);
		addDebugLine(frustum.points[5], frustum.points[6], color, life);
		addDebugLine(frustum.points[6], frustum.points[7], color, life);
		addDebugLine(frustum.points[7], frustum.points[4], color, life);

		addDebugLine(frustum.points[0], frustum.points[4], color, life);
		addDebugLine(frustum.points[1], frustum.points[5], color, life);
		addDebugLine(frustum.points[2], frustum.points[6], color, life);
		addDebugLine(frustum.points[3], frustum.points[7], color, life);
	}


	void addDebugCircle(const Vec3& center, const Vec3& up, float radius, u32 color, float life) override
	{
		Vec3 z_vec(-up.y, up.x, 0);
		Vec3 x_vec = crossProduct(up, z_vec);
		float prevx = radius;
		float prevz = 0;
		z_vec.normalize();
		x_vec.normalize();
		for (int i = 1; i <= 64; ++i)
		{
			float a = i / 64.0f * 2 * Math::PI;
			float x = cosf(a) * radius;
			float z = sinf(a) * radius;
			addDebugLine(center + x_vec * x + z_vec * z, center + x_vec * prevx + z_vec * prevz, color, life);
			prevx = x;
			prevz = z;
		}
	}


	void addDebugCross(const Vec3& center, float size, u32 color, float life) override
	{
		addDebugLine(center, Vec3(center.x - size, center.y, center.z), color, life);
		addDebugLine(center, Vec3(center.x + size, center.y, center.z), color, life);
		addDebugLine(center, Vec3(center.x, center.y - size, center.z), color, life);
		addDebugLine(center, Vec3(center.x, center.y + size, center.z), color, life);
		addDebugLine(center, Vec3(center.x, center.y, center.z - size), color, life);
		addDebugLine(center, Vec3(center.x, center.y, center.z + size), color, life);
	}


	void addDebugPoint(const Vec3& pos, u32 color, float life) override
	{
		DebugPoint& point = m_debug_points.emplace();
		point.pos = pos;
		point.color = ARGBToABGR(color);
		point.life = life;
	}


	void addDebugCone(const Vec3& vertex,
		const Vec3& dir,
		const Vec3& axis0,
		const Vec3& axis1,
		u32 color,
		float life) override
	{
		Vec3 base_center = vertex + dir;
		Vec3 prev_p = base_center + axis0;
		for (int i = 1; i <= 32; ++i)
		{
			float angle = i / 32.0f * 2 * Math::PI;
			Vec3 x = cosf(angle) * axis0;
			Vec3 z = sinf(angle) * axis1;
			Vec3 p = base_center + x + z;
			addDebugLine(p, prev_p, color, life);
			addDebugLine(vertex, p, color, life);
			prev_p = p;
		}
	}


	static u32 ARGBToABGR(u32 color)
	{
		return ((color & 0xff) << 16) | (color & 0xff00) | ((color & 0xff0000) >> 16) | (color & 0xff000000);
	}


	void addDebugLine(const Vec3& from, const Vec3& to, u32 color, float life) override
	{
		DebugLine& line = m_debug_lines.emplace();
		line.from = from;
		line.to = to;
		line.color = ARGBToABGR(color);
		line.life = life;
	}


	RayCastModelHit castRayTerrain(GameObject gameobject, const Vec3& origin, const Vec3& dir) override
	{
		RayCastModelHit hit;
		hit.m_is_hit = false;
		auto iter = m_terrains.find(gameobject);
		if (!iter.isValid()) return hit;

		auto* terrain = iter.value();
		hit = terrain->castRay(origin, dir);
		hit.m_component_type = TERRAIN_TYPE;
		hit.m_gameobject = terrain->getGameObject();
		return hit;
	}


	RayCastModelHit castRay(const Vec3& origin, const Vec3& dir, GameObject ignored_model_instance) override
	{
		PROFILE_FUNCTION();
		RayCastModelHit hit;
		hit.m_is_hit = false;
		hit.m_origin = origin;
		hit.m_dir = dir;
		float cur_dist = FLT_MAX;
		Project& project = getProject();
		for (int i = 0; i < m_model_instances.size(); ++i)
		{
			auto& r = m_model_instances[i];
			if (ignored_model_instance.index == i || !r.model) continue;
			if (!r.flags.isSet(ModelInstance::ENABLED)) continue;

			const Vec3& pos = r.matrix.getTranslation();
			float scale = project.getScale(r.gameobject);
			float radius = r.model->getBoundingRadius() * scale;
			float dist = (pos - origin).length();
			if (dist - radius > cur_dist) continue;
			
			Vec3 intersection;
			if (Math::getRaySphereIntersection(origin, dir, pos, radius, intersection))
			{
				RayCastModelHit new_hit = r.model->castRay(origin, dir, r.matrix, r.pose);
				if (new_hit.m_is_hit && (!hit.m_is_hit || new_hit.m_t < hit.m_t))
				{
					new_hit.m_gameobject = r.gameobject;
					new_hit.m_component_type = MODEL_INSTANCE_TYPE;
					hit = new_hit;
					hit.m_is_hit = true;
					cur_dist = dir.length() * hit.m_t;
				}
			}
		}

		for (auto* terrain : m_terrains)
		{
			RayCastModelHit terrain_hit = terrain->castRay(origin, dir);
			if (terrain_hit.m_is_hit && (!hit.m_is_hit || terrain_hit.m_t < hit.m_t))
			{
				terrain_hit.m_component_type = TERRAIN_TYPE;
				terrain_hit.m_gameobject = terrain->getGameObject();
				terrain_hit.m_mesh = nullptr;
				hit = terrain_hit;
			}
		}

		return hit;
	}

	
	Vec4 getShadowmapCascades(GameObject gameobject) override
	{
		return m_global_lights[gameobject].m_cascades;
	}


	void setShadowmapCascades(GameObject gameobject, const Vec4& value) override
	{
		Vec4 valid_value = value;
		valid_value.x = Math::maximum(valid_value.x, 0.02f);
		valid_value.y = Math::maximum(valid_value.x + 0.01f, valid_value.y);
		valid_value.z = Math::maximum(valid_value.y + 0.01f, valid_value.z);
		valid_value.w = Math::maximum(valid_value.z + 0.01f, valid_value.w);

		m_global_lights[gameobject].m_cascades = valid_value;
	}


	void setFogDensity(GameObject gameobject, float density) override
	{
		m_global_lights[gameobject].m_fog_density = density;
	}


	void setFogColor(GameObject gameobject, const Vec3& color) override
	{
		m_global_lights[gameobject].m_fog_color = color;
	}


	float getFogDensity(GameObject gameobject) override
	{
		return m_global_lights[gameobject].m_fog_density;
	}


	float getFogBottom(GameObject gameobject) override
	{
		return m_global_lights[gameobject].m_fog_bottom;
	}


	void setFogBottom(GameObject gameobject, float bottom) override
	{
		m_global_lights[gameobject].m_fog_bottom = bottom;
	}


	float getFogHeight(GameObject gameobject) override
	{
		return m_global_lights[gameobject].m_fog_height;
	}


	void setFogHeight(GameObject gameobject, float height) override
	{
		m_global_lights[gameobject].m_fog_height = height;
	}


	Vec3 getFogColor(GameObject gameobject) override
	{
		return m_global_lights[gameobject].m_fog_color;
	}


	float getLightAttenuation(GameObject gameobject) override
	{
		return m_point_lights[m_point_lights_map[gameobject]].m_attenuation_param;
	}


	void setLightAttenuation(GameObject gameobject, float attenuation) override
	{
		int index = m_point_lights_map[gameobject];
		m_point_lights[index].m_attenuation_param = attenuation;
	}


	float getLightRange(GameObject gameobject) override
	{
		return m_point_lights[m_point_lights_map[gameobject]].m_range;
	}


	void setLightRange(GameObject gameobject, float value) override
	{
		m_point_lights[m_point_lights_map[gameobject]].m_range = value;
	}


	void setPointLightIntensity(GameObject gameobject, float intensity) override
	{
		m_point_lights[m_point_lights_map[gameobject]].m_diffuse_intensity = intensity;
	}


	void setGlobalLightIntensity(GameObject gameobject, float intensity) override
	{
		m_global_lights[gameobject].m_diffuse_intensity = intensity;
	}


	void setGlobalLightIndirectIntensity(GameObject gameobject, float intensity) override
	{
		m_global_lights[gameobject].m_indirect_intensity = intensity;
	}


	void setPointLightColor(GameObject gameobject, const Vec3& color) override
	{
		m_point_lights[m_point_lights_map[gameobject]].m_diffuse_color = color;
	}


	void setGlobalLightColor(GameObject gameobject, const Vec3& color) override
	{
		m_global_lights[gameobject].m_diffuse_color = color;
	}

	
	float getPointLightIntensity(GameObject gameobject) override
	{
		return m_point_lights[m_point_lights_map[gameobject]].m_diffuse_intensity;
	}


	float getGlobalLightIntensity(GameObject gameobject) override
	{
		return m_global_lights[gameobject].m_diffuse_intensity;
	}


	float getGlobalLightIndirectIntensity(GameObject gameobject) override
	{
		return m_global_lights[gameobject].m_indirect_intensity;
	}


	Vec3 getPointLightColor(GameObject gameobject) override
	{
		return m_point_lights[m_point_lights_map[gameobject]].m_diffuse_color;
	}


	void setPointLightSpecularColor(GameObject gameobject, const Vec3& color) override
	{
		m_point_lights[m_point_lights_map[gameobject]].m_specular_color = color;
	}


	Vec3 getPointLightSpecularColor(GameObject gameobject) override
	{
		return m_point_lights[m_point_lights_map[gameobject]].m_specular_color;
	}


	void setPointLightSpecularIntensity(GameObject gameobject, float intensity) override
	{
		m_point_lights[m_point_lights_map[gameobject]].m_specular_intensity = intensity;
	}


	float getPointLightSpecularIntensity(GameObject gameobject) override
	{
		return m_point_lights[m_point_lights_map[gameobject]].m_specular_intensity;
	}


	Vec3 getGlobalLightColor(GameObject gameobject) override
	{
		return m_global_lights[gameobject].m_diffuse_color;
	}


	void setActiveGlobalLight(GameObject gameobject) override
	{
		m_active_global_light_gameobject = gameobject;
	}


	GameObject getActiveGlobalLight() override
	{
		return m_active_global_light_gameobject;
	}


	GameObject getPointLightGameObject(GameObject gameobject) const override
	{
		return m_point_lights[m_point_lights_map[gameobject]].m_gameobject;
	}


	GameObject getGlobalLightGameObject(GameObject gameobject) const override
	{
		return m_global_lights[gameobject].m_gameobject;
	}


	void reloadEnvironmentProbe(GameObject gameobject) override
	{
		auto& probe = m_environment_probes[gameobject];
		auto* texture_manager = m_engine.getResourceManager().get(Texture::TYPE);
		if (probe.texture) texture_manager->unload(*probe.texture);
		probe.texture = nullptr;
		StaticString<MAX_PATH_LENGTH> path;
		if (probe.flags.isSet(EnvironmentProbe::REFLECTION))
		{
			path  << "projects/" << m_project.getName() << "/probes/" << probe.guid << ".dds";
			probe.texture = static_cast<Texture*>(texture_manager->load(Path(path)));
			probe.texture->setFlag(BGFX_TEXTURE_SRGB, true);
		}
		path = "projects/";
		path << m_project.getName() << "/probes/" << probe.guid << "_irradiance.dds";
		probe.irradiance = static_cast<Texture*>(texture_manager->load(Path(path)));
		probe.irradiance->setFlag(BGFX_TEXTURE_SRGB, true);
		probe.irradiance->setFlag(BGFX_TEXTURE_MIN_ANISOTROPIC, true);
		probe.irradiance->setFlag(BGFX_TEXTURE_MAG_ANISOTROPIC, true);
		path = "projects/";
		path << m_project.getName() << "/probes/" << probe.guid << "_radiance.dds";
		probe.radiance = static_cast<Texture*>(texture_manager->load(Path(path)));
		probe.radiance->setFlag(BGFX_TEXTURE_SRGB, true);
		probe.radiance->setFlag(BGFX_TEXTURE_MIN_ANISOTROPIC, true);
		probe.radiance->setFlag(BGFX_TEXTURE_MAG_ANISOTROPIC, true);
	}


	GameObject getNearestEnvironmentProbe(const Vec3& pos) const override
	{
		float nearest_dist_squared = FLT_MAX;
		GameObject nearest = INVALID_GAMEOBJECT;
		for (int i = 0, c = m_environment_probes.size(); i < c; ++i)
		{
			GameObject probe_gameobject = m_environment_probes.getKey(i);
			Vec3 probe_pos = m_project.getPosition(probe_gameobject);
			float dist_squared = (pos - probe_pos).squaredLength();
			if (dist_squared < nearest_dist_squared)
			{
				nearest = probe_gameobject;
				nearest_dist_squared = dist_squared;
			}
		}
		if (!nearest.isValid()) return INVALID_GAMEOBJECT;
		return nearest;
	}


	int getEnvironmentProbeIrradianceSize(GameObject gameobject)
	{
		return m_environment_probes[gameobject].irradiance_size;
	}


	void setEnvironmentProbeIrradianceSize(GameObject gameobject, int size)
	{
		m_environment_probes[gameobject].irradiance_size = size;
	}


	int getEnvironmentProbeRadianceSize(GameObject gameobject)
	{
		return m_environment_probes[gameobject].radiance_size;
	}


	void setEnvironmentProbeRadianceSize(GameObject gameobject, int size)
	{
		m_environment_probes[gameobject].radiance_size = size;
	}


	int getEnvironmentProbeReflectionSize(GameObject gameobject)
	{
		return m_environment_probes[gameobject].reflection_size;
	}


	void setEnvironmentProbeReflectionSize(GameObject gameobject, int size)
	{
		m_environment_probes[gameobject].reflection_size = size;
	}


	bool isEnvironmentProbeCustomSize(GameObject gameobject) override
	{
		return m_environment_probes[gameobject].flags.isSet(EnvironmentProbe::OVERRIDE_GLOBAL_SIZE);
	}


	void enableEnvironmentProbeCustomSize(GameObject gameobject, bool enable) override
	{
		m_environment_probes[gameobject].flags.set(EnvironmentProbe::OVERRIDE_GLOBAL_SIZE, enable);
	}


	bool isEnvironmentProbeReflectionEnabled(GameObject gameobject) override
	{
		return m_environment_probes[gameobject].flags.isSet(EnvironmentProbe::REFLECTION);
	}


	void enableEnvironmentProbeReflection(GameObject gameobject, bool enable) override
	{
		m_environment_probes[gameobject].flags.set(EnvironmentProbe::REFLECTION, enable);
	}


	Texture* getEnvironmentProbeTexture(GameObject gameobject) const override
	{
		return m_environment_probes[gameobject].texture;
	}


	Texture* getEnvironmentProbeIrradiance(GameObject gameobject) const override
	{
		return m_environment_probes[gameobject].irradiance;
	}


	Texture* getEnvironmentProbeRadiance(GameObject gameobject) const override
	{
		return m_environment_probes[gameobject].radiance;
	}


	u64 getEnvironmentProbeGUID(GameObject gameobject) const override
	{
		return m_environment_probes[gameobject].guid;
	}


	GameObject getCameraInSlot(const char* slot) override
	{
		for (const auto& camera : m_cameras)
		{
			if (equalStrings(camera.slot, slot))
			{
				return camera.gameobject;
			}
		}
		return INVALID_GAMEOBJECT;
	}


	float getTime() const override { return m_time; }


	void modelUnloaded(Model*, GameObject gameobject)
	{
		auto& r = m_model_instances[gameobject.index];
		if (!hasCustomMeshes(r))
		{
			r.meshes = nullptr;
			r.mesh_count = 0;
		}
		MALMY_DELETE(m_allocator, r.pose);
		r.pose = nullptr;

		for (int i = 0; i < m_point_lights.size(); ++i)
		{
			m_light_influenced_geometry[i].eraseItemFast(gameobject);
		}
		m_culling_system->removeStatic(gameobject);
	}


	void freeCustomMeshes(ModelInstance& r, MaterialManager* manager)
	{
		if (!hasCustomMeshes(r)) return;
		for (int i = 0; i < r.mesh_count; ++i)
		{
			manager->unload(*r.meshes[i].material);
			r.meshes[i].~Mesh();
		}
		m_allocator.deallocate(r.meshes);
		r.meshes = nullptr;
		r.flags.unset(ModelInstance::CUSTOM_MESHES);
		r.mesh_count = 0;
	}


	void modelLoaded(Model* model, GameObject gameobject)
	{
		auto& rm = m_engine.getResourceManager();
		auto* material_manager = static_cast<MaterialManager*>(rm.get(Material::TYPE));

		auto& r = m_model_instances[gameobject.index];

		float bounding_radius = r.model->getBoundingRadius();
		float scale = m_project.getScale(r.gameobject);
		Sphere sphere(r.matrix.getTranslation(), bounding_radius * scale);
		if(r.flags.isSet(ModelInstance::ENABLED)) m_culling_system->addStatic(gameobject, sphere, getLayerMask(r));
		ASSERT(!r.pose);
		if (model->getBoneCount() > 0)
		{
			r.pose = MALMY_NEW(m_allocator, Pose)(m_allocator);
			r.pose->resize(model->getBoneCount());
			model->getPose(*r.pose);
			int skinned_define_idx = m_renderer.getShaderDefineIdx("SKINNED");
			for (int i = 0; i < model->getMeshCount(); ++i)
			{
				Mesh& mesh = model->getMesh(i);
				mesh.material->setDefine(skinned_define_idx, !mesh.skin.empty());
			}
		}
		r.matrix = m_project.getMatrix(r.gameobject);
		ASSERT(!r.meshes || hasCustomMeshes(r));
		if (r.meshes)
		{
			allocateCustomMeshes(r, model->getMeshCount());
			for (int i = 0; i < r.mesh_count; ++i)
			{
				auto& src = model->getMesh(i);
				if (!r.meshes[i].material)
				{
					material_manager->load(*src.material);
					r.meshes[i].material = src.material;
				}
				r.meshes[i].set(src);
			}
		}
		else
		{
			r.meshes = &r.model->getMesh(0);
			r.mesh_count = r.model->getMeshCount();
		}

		if (r.flags.isSet(ModelInstance::IS_BONE_ATTACHMENT_PARENT))
		{
			updateBoneAttachment(m_bone_attachments[r.gameobject]);
		}

		for (int i = 0; i < m_point_lights.size(); ++i)
		{
			PointLight& light = m_point_lights[i];
			Vec3 t = r.matrix.getTranslation();
			float radius = r.model->getBoundingRadius();
			if ((t - m_project.getPosition(light.m_gameobject)).squaredLength() <
				(radius + light.m_range) * (radius + light.m_range))
			{
				m_light_influenced_geometry[i].push(gameobject);
			}
		}
	}


	void modelUnloaded(Model* model)
	{
		for (int i = 0, c = m_model_instances.size(); i < c; ++i)
		{
			if (m_model_instances[i].gameobject != INVALID_GAMEOBJECT && m_model_instances[i].model == model)
			{
				modelUnloaded(model, {i});
			}
		}
	}


	void modelLoaded(Model* model)
	{
		for (int i = 0, c = m_model_instances.size(); i < c; ++i)
		{
			if (m_model_instances[i].gameobject != INVALID_GAMEOBJECT && m_model_instances[i].model == model)
			{
				modelLoaded(model, {i});
			}
		}
	}


	ModelLoadedCallback& getModelLoadedCallback(Model* model)
	{
		int idx = m_model_loaded_callbacks.find(model);
		if (idx >= 0) return m_model_loaded_callbacks.at(idx);
		return m_model_loaded_callbacks.emplace(model, *this, model);
	}


	void allocateCustomMeshes(ModelInstance& r, int count)
	{
		if (hasCustomMeshes(r) && r.mesh_count == count) return;

		ASSERT(r.model);
		auto& rm = r.model->getResourceManager();
		auto* material_manager = static_cast<MaterialManager*>(rm.getOwner().get(Material::TYPE));

		auto* new_meshes = (Mesh*)m_allocator.allocate(count * sizeof(Mesh));
		if (r.meshes)
		{
			for (int i = 0; i < r.mesh_count; ++i)
			{
				new (NewPlaceholder(), new_meshes + i) Mesh(r.meshes[i]);
			}

			if (hasCustomMeshes(r))
			{
				for (int i = count; i < r.mesh_count; ++i)
				{
					material_manager->unload(*r.meshes[i].material);
				}
				for (int i = 0; i < r.mesh_count; ++i)
				{
					r.meshes[i].~Mesh();
				}
				m_allocator.deallocate(r.meshes);
			}
			else
			{
				for (int i = 0; i < r.mesh_count; ++i)
				{
					material_manager->load(*r.meshes[i].material);
				}
			}
		}

		for (int i = r.mesh_count; i < count; ++i)
		{
			new (NewPlaceholder(), new_meshes + i) Mesh(nullptr, bgfx::VertexDecl(), "", m_allocator);
		}
		r.meshes = new_meshes;
		r.mesh_count = count;
		r.flags.set(ModelInstance::CUSTOM_MESHES);
	}


	void setModelInstanceMaterial(GameObject gameobject, int index, const Path& path) override
	{
		auto& r = m_model_instances[gameobject.index];
		if (r.meshes && r.mesh_count > index && r.meshes[index].material && path == r.meshes[index].material->getPath()) return;

		auto& rm = r.model->getResourceManager();
		auto* material_manager = static_cast<MaterialManager*>(rm.getOwner().get(Material::TYPE));

		int new_count = Math::maximum(i8(index + 1), r.mesh_count);
		allocateCustomMeshes(r, new_count);
		ASSERT(r.meshes);

		Material* new_material = static_cast<Material*>(material_manager->load(path));

		const int skinned_define_idx = m_renderer.getShaderDefineIdx("SKINNED");
		new_material->setDefine(skinned_define_idx, !r.meshes[index].skin.empty());

		r.meshes[index].setMaterial(new_material, *r.model, m_renderer);
	}


	Path getModelInstanceMaterial(GameObject gameobject, int index) override
	{
		auto& r = m_model_instances[gameobject.index];
		if (!r.meshes) return Path("");

		return r.meshes[index].material->getPath();
	}


	void setModel(GameObject gameobject, Model* model)
	{
		auto& model_instance = m_model_instances[gameobject.index];
		ASSERT(model_instance.gameobject.isValid());
		Model* old_model = model_instance.model;
		bool no_change = model == old_model && old_model;
		if (no_change)
		{
			old_model->getResourceManager().unload(*old_model);
			return;
		}
		if (old_model)
		{
			auto& rm = old_model->getResourceManager();
			auto* material_manager = static_cast<MaterialManager*>(rm.getOwner().get(Material::TYPE));
			freeCustomMeshes(model_instance, material_manager);
			ModelLoadedCallback& callback = getModelLoadedCallback(old_model);
			--callback.m_ref_count;
			if (callback.m_ref_count == 0)
			{
				m_model_loaded_callbacks.erase(old_model);
			}

			if (old_model->isReady())
			{
				m_culling_system->removeStatic(gameobject);
			}
			old_model->getResourceManager().unload(*old_model);
		}
		model_instance.model = model;
		model_instance.meshes = nullptr;
		model_instance.mesh_count = 0;
		MALMY_DELETE(m_allocator, model_instance.pose);
		model_instance.pose = nullptr;
		if (model)
		{
			ModelLoadedCallback& callback = getModelLoadedCallback(model);
			++callback.m_ref_count;

			if (model->isReady())
			{
				modelLoaded(model, gameobject);
			}
		}
	}

	IAllocator& getAllocator() override { return m_allocator; }


	void detectLightInfluencedGeometry(GameObject gameobject)
	{
		int light_idx = m_point_lights_map[gameobject];
		Frustum frustum = getPointLightFrustum(light_idx);
		const CullingSystem::Results& results = m_culling_system->cull(frustum, ~0ULL);
		auto& influenced_geometry = m_light_influenced_geometry[light_idx];
		influenced_geometry.clear();
		for (int i = 0; i < results.size(); ++i)
		{
			const CullingSystem::Subresults& subresult = results[i];
			influenced_geometry.reserve(influenced_geometry.size() + subresult.size());
			for (int j = 0, c = subresult.size(); j < c; ++j)
			{
				influenced_geometry.push(subresult[j]);
			}
		}
	}


	int getParticleEmitterAttractorCount(GameObject gameobject) override
	{
		auto* module = getEmitterModule<ParticleEmitter::AttractorModule>(gameobject);
		return module ? module->m_count : 0;
	}


	void addParticleEmitterAttractor(GameObject gameobject, int index) override
	{
		auto* module = getEmitterModule<ParticleEmitter::AttractorModule>(gameobject);
		if (!module) return;

		auto* plane_module = static_cast<ParticleEmitter::AttractorModule*>(module);
		if (plane_module->m_count == lengthOf(plane_module->m_entities)) return;

		if (index < 0)
		{
			plane_module->m_entities[plane_module->m_count] = INVALID_GAMEOBJECT;
			++plane_module->m_count;
			return;
		}

		for (int i = plane_module->m_count - 1; i > index; --i)
		{
			plane_module->m_entities[i] = plane_module->m_entities[i - 1];
		}
		plane_module->m_entities[index] = INVALID_GAMEOBJECT;
		++plane_module->m_count;
	}


	void removeParticleEmitterAttractor(GameObject gameobject, int index) override
	{
		auto* module = getEmitterModule<ParticleEmitter::AttractorModule>(gameobject);
		if (!module) return;

		for (int i = index; i < module->m_count - 1; ++i)
		{
			module->m_entities[i] = module->m_entities[i + 1];
		}
		--module->m_count;
	}


	GameObject getParticleEmitterAttractorGameObject(GameObject gameobject, int index) override
	{
		auto* module = getEmitterModule<ParticleEmitter::AttractorModule>(gameobject);
		return module ? module->m_entities[index] : INVALID_GAMEOBJECT;
	}


	void setParticleEmitterAttractorGameObject(GameObject module_gameobject, int index, GameObject gameobject) override
	{
		auto* module = getEmitterModule<ParticleEmitter::AttractorModule>(module_gameobject);
		if(module) module->m_entities[index] = gameobject;
	}


	float getParticleEmitterShapeRadius(GameObject gameobject) override
	{
		auto* module = getEmitterModule<ParticleEmitter::SpawnShapeModule>(gameobject);
		return module ? module->m_radius : 0.0f;
	}


	void setParticleEmitterShapeRadius(GameObject gameobject, float value) override
	{
		auto* module = getEmitterModule<ParticleEmitter::SpawnShapeModule>(gameobject);
		if (module) module->m_radius = value;
	}


	int getParticleEmitterPlaneCount(GameObject gameobject) override
	{
		auto* module = getEmitterModule<ParticleEmitter::PlaneModule>(gameobject);
		return module ? module->m_count : 0;
	}


	void addParticleEmitterPlane(GameObject gameobject, int index) override
	{
		auto* plane_module = getEmitterModule<ParticleEmitter::PlaneModule>(gameobject);
		if (!plane_module) return;

		if (plane_module->m_count == lengthOf(plane_module->m_entities)) return;

		if (index < 0)
		{
			plane_module->m_entities[plane_module->m_count] = INVALID_GAMEOBJECT;
			++plane_module->m_count;
			return;
		}

		for (int i = plane_module->m_count - 1; i > index; --i)
		{
			plane_module->m_entities[i] = plane_module->m_entities[i - 1];
		}
		plane_module->m_entities[index] = INVALID_GAMEOBJECT;
		++plane_module->m_count;
			
	}


	void removeParticleEmitterPlane(GameObject gameobject, int index) override
	{
		auto* plane_module = getEmitterModule<ParticleEmitter::PlaneModule>(gameobject);
		if (!plane_module) return;

		for (int i = index; i < plane_module->m_count - 1; ++i)
		{
			plane_module->m_entities[i] = plane_module->m_entities[i + 1];
		}
		--plane_module->m_count;
	}


	GameObject getParticleEmitterPlaneGameObject(GameObject gameobject, int index) override
	{
		auto* module = getEmitterModule<ParticleEmitter::PlaneModule>(gameobject);
		return module ? module->m_entities[index] : INVALID_GAMEOBJECT;
	}


	void setParticleEmitterPlaneGameObject(GameObject module_gameobject, int index, GameObject gameobject) override
	{
		auto* module = getEmitterModule<ParticleEmitter::PlaneModule>(module_gameobject);
		if (module) module->m_entities[index] = gameobject;
	}


	float getLightFOV(GameObject gameobject) override
	{
		return m_point_lights[m_point_lights_map[gameobject]].m_fov;
	}


	void setLightFOV(GameObject gameobject, float fov) override
	{
		m_point_lights[m_point_lights_map[gameobject]].m_fov = fov;
	}


	void createGlobalLight(GameObject gameobject)
	{
		GlobalLight light;
		light.m_gameobject = gameobject;
		light.m_diffuse_color.set(1, 1, 1);
		light.m_diffuse_intensity = 0;
		light.m_indirect_intensity = 1;
		light.m_fog_color.set(1, 1, 1);
		light.m_fog_density = 0;
		light.m_cascades.set(3, 8, 100, 300);
		light.m_fog_bottom = 0.0f;
		light.m_fog_height = 10.0f;

		if (m_global_lights.empty()) m_active_global_light_gameobject = gameobject;

		m_global_lights.insert(gameobject, light);
		m_project.onComponentCreated(gameobject, GLOBAL_LIGHT_TYPE, this);
	}


	void createPointLight(GameObject gameobject)
	{
		PointLight& light = m_point_lights.emplace();
		m_light_influenced_geometry.emplace(m_allocator);
		light.m_gameobject = gameobject;
		light.m_diffuse_color.set(1, 1, 1);
		light.m_diffuse_intensity = 1;
		light.m_fov = Math::degreesToRadians(360);
		light.m_specular_color.set(1, 1, 1);
		light.m_specular_intensity = 1;
		light.m_cast_shadows = false;
		light.m_attenuation_param = 2;
		light.m_range = 10;
		m_point_lights_map.insert(gameobject, m_point_lights.size() - 1);

		m_project.onComponentCreated(gameobject, POINT_LIGHT_TYPE, this);

		detectLightInfluencedGeometry(gameobject);
	}


	void updateDecalInfo(Decal& decal) const
	{
		decal.position = m_project.getPosition(decal.gameobject);
		decal.radius = decal.scale.length();
		decal.mtx = m_project.getMatrix(decal.gameobject);
		decal.mtx.setXVector(decal.mtx.getXVector() * decal.scale.x);
		decal.mtx.setYVector(decal.mtx.getYVector() * decal.scale.y);
		decal.mtx.setZVector(decal.mtx.getZVector() * decal.scale.z);
		decal.inv_mtx = decal.mtx;
		decal.inv_mtx.inverse();
	}


	void createDecal(GameObject gameobject)
	{
		Decal& decal = m_decals.insert(gameobject);
		decal.material = nullptr;
		decal.gameobject = gameobject;
		decal.scale.set(1, 1, 1);
		updateDecalInfo(decal);

		m_project.onComponentCreated(gameobject, DECAL_TYPE, this);
	}


	void createEnvironmentProbe(GameObject gameobject)
	{
		EnvironmentProbe& probe = m_environment_probes.insert(gameobject);
		auto* texture_manager = m_engine.getResourceManager().get(Texture::TYPE);
		probe.texture = static_cast<Texture*>(texture_manager->load(Path("pipelines/pbr/default_probe.dds")));
		probe.texture->setFlag(BGFX_TEXTURE_SRGB, true);
		probe.irradiance = static_cast<Texture*>(texture_manager->load(Path("pipelines/pbr/default_probe.dds")));
		probe.irradiance->setFlag(BGFX_TEXTURE_SRGB, true);
		probe.radiance = static_cast<Texture*>(texture_manager->load(Path("pipelines/pbr/default_probe.dds")));
		probe.radiance->setFlag(BGFX_TEXTURE_SRGB, true);
		probe.guid = Math::randGUID();

		m_project.onComponentCreated(gameobject, ENVIRONMENT_PROBE_TYPE, this);
	}


	void createBoneAttachment(GameObject gameobject)
	{
		BoneAttachment& attachment = m_bone_attachments.emplace(gameobject);
		attachment.gameobject = gameobject;
		attachment.parent_gameobject = INVALID_GAMEOBJECT;
		attachment.bone_index = -1;

		m_project.onComponentCreated(gameobject, BONE_ATTACHMENT_TYPE, this);
	}


	void createModelInstance(GameObject gameobject)
	{
		while(gameobject.index >= m_model_instances.size())
		{
			auto& r = m_model_instances.emplace();
			r.gameobject = INVALID_GAMEOBJECT;
			r.model = nullptr;
			r.pose = nullptr;
		}
		auto& r = m_model_instances[gameobject.index];
		r.gameobject = gameobject;
		r.model = nullptr;
		r.meshes = nullptr;
		r.pose = nullptr;
		r.flags.clear();
		r.flags.set(ModelInstance::ENABLED);
		r.mesh_count = 0;
		r.matrix = m_project.getMatrix(gameobject);
		m_project.onComponentCreated(gameobject, MODEL_INSTANCE_TYPE, this);
	}


	void setScriptedParticleEmitterMaterialPath(GameObject gameobject, const Path& path) override
	{
		if (!m_scripted_particle_emitters[gameobject]) return;

		auto* manager = m_engine.getResourceManager().get(Material::TYPE);
		Material* material = static_cast<Material*>(manager->load(path));
		m_scripted_particle_emitters[gameobject]->setMaterial(material);
	}


	Path getScriptedParticleEmitterMaterialPath(GameObject gameobject) override
	{
		ScriptedParticleEmitter* emitter = m_scripted_particle_emitters[gameobject];
		if (!emitter) return Path("");
		if (!emitter->getMaterial()) return Path("");

		return emitter->getMaterial()->getPath();
	}

	void setParticleEmitterMaterialPath(GameObject gameobject, const Path& path) override
	{
		if (!m_particle_emitters[gameobject]) return;

		auto* manager = m_engine.getResourceManager().get(Material::TYPE);
		Material* material = static_cast<Material*>(manager->load(path));
		m_particle_emitters[gameobject]->setMaterial(material);
	}


	Path getParticleEmitterMaterialPath(GameObject gameobject) override
	{
		ParticleEmitter* emitter = m_particle_emitters[gameobject];
		if (!emitter) return Path("");
		if (!emitter->getMaterial()) return Path("");

		return emitter->getMaterial()->getPath();
	}


	const AssociativeArray<GameObject, ParticleEmitter*>& getParticleEmitters() const override
	{
		return m_particle_emitters;
	}

	const AssociativeArray<GameObject, ScriptedParticleEmitter*>& getScriptedParticleEmitters() const override
	{
		return m_scripted_particle_emitters;
	}

private:
	IAllocator& m_allocator;
	Project& m_project;
	Renderer& m_renderer;
	Engine& m_engine;
	CullingSystem* m_culling_system;

	Array<Array<GameObject>> m_light_influenced_geometry;
	GameObject m_active_global_light_gameobject;
	HashMap<GameObject, int> m_point_lights_map;

	AssociativeArray<GameObject, Decal> m_decals;
	Array<ModelInstance> m_model_instances;
	HashMap<GameObject, GlobalLight> m_global_lights;
	Array<PointLight> m_point_lights;
	HashMap<GameObject, Camera> m_cameras;
	AssociativeArray<GameObject, TextMesh*> m_text_meshes;
	AssociativeArray<GameObject, BoneAttachment> m_bone_attachments;
	AssociativeArray<GameObject, EnvironmentProbe> m_environment_probes;
	HashMap<GameObject, Terrain*> m_terrains;
	AssociativeArray<GameObject, ParticleEmitter*> m_particle_emitters;
	AssociativeArray<GameObject, ScriptedParticleEmitter*> m_scripted_particle_emitters;

	Array<DebugTriangle> m_debug_triangles;
	Array<DebugLine> m_debug_lines;
	Array<DebugPoint> m_debug_points;

	Array<Array<MeshInstance>> m_temporary_infos;

	float m_time;
	float m_lod_multiplier;
	bool m_is_updating_attachments;
	bool m_is_grass_enabled;
	bool m_is_game_running;

	AssociativeArray<Model*, ModelLoadedCallback> m_model_loaded_callbacks;
};



#define COMPONENT_TYPE(type, name) \
	{ \
		type \
		, static_cast<Project::Serialize>(&RenderSceneImpl::serialize##name) \
		, static_cast<Project::Deserialize>(&RenderSceneImpl::deserialize##name) \
		, &RenderSceneImpl::create##name \
		, &RenderSceneImpl::destroy##name \
	}

static struct
{
	ComponentType type;
	Project::Serialize serialize;
	Project::Deserialize deserialize;
	void (RenderSceneImpl::*creator)(GameObject);
	void (RenderSceneImpl::*destroyer)(GameObject);
} COMPONENT_INFOS[] = {
	COMPONENT_TYPE(MODEL_INSTANCE_TYPE, ModelInstance),
	COMPONENT_TYPE(GLOBAL_LIGHT_TYPE, GlobalLight),
	COMPONENT_TYPE(POINT_LIGHT_TYPE, PointLight),
	COMPONENT_TYPE(DECAL_TYPE, Decal),
	COMPONENT_TYPE(CAMERA_TYPE, Camera),
	COMPONENT_TYPE(TERRAIN_TYPE, Terrain),
	COMPONENT_TYPE(BONE_ATTACHMENT_TYPE, BoneAttachment),
	COMPONENT_TYPE(ENVIRONMENT_PROBE_TYPE, EnvironmentProbe),
	COMPONENT_TYPE(PARTICLE_EMITTER_TYPE, ParticleEmitter),
	COMPONENT_TYPE(SCRIPTED_PARTICLE_EMITTER_TYPE, ScriptedParticleEmitter),
	COMPONENT_TYPE(PARTICLE_EMITTER_ALPHA_TYPE, ParticleEmitterAlpha),
	COMPONENT_TYPE(PARTICLE_EMITTER_ATTRACTOR_TYPE, ParticleEmitterAttractor),
	COMPONENT_TYPE(PARTICLE_EMITTER_FORCE_HASH, ParticleEmitterForce),
	COMPONENT_TYPE(PARTICLE_EMITTER_LINEAR_MOVEMENT_TYPE, ParticleEmitterLinearMovement),
	COMPONENT_TYPE(PARTICLE_EMITTER_PLANE_TYPE, ParticleEmitterPlane),
	COMPONENT_TYPE(PARTICLE_EMITTER_RANDOM_ROTATION_TYPE, ParticleEmitterRandomRotation),
	COMPONENT_TYPE(PARTICLE_EMITTER_SIZE_TYPE, ParticleEmitterSize),
	COMPONENT_TYPE(PARTICLE_EMITTER_SPAWN_SHAPE_TYPE, ParticleEmitterSpawnShape),
	COMPONENT_TYPE(PARTICLE_EMITTER_SUBIMAGE_TYPE, ParticleEmitterSubimage),
	COMPONENT_TYPE(TEXT_MESH_TYPE, TextMesh)
};

#undef COMPONENT_TYPE

RenderSceneImpl::RenderSceneImpl(Renderer& renderer,
	Engine& engine,
	Project& project,
	IAllocator& allocator)
	: m_engine(engine)
	, m_project(project)
	, m_renderer(renderer)
	, m_allocator(allocator)
	, m_model_loaded_callbacks(m_allocator)
	, m_model_instances(m_allocator)
	, m_cameras(m_allocator)
	, m_text_meshes(m_allocator)
	, m_terrains(m_allocator)
	, m_point_lights(m_allocator)
	, m_light_influenced_geometry(m_allocator)
	, m_global_lights(m_allocator)
	, m_decals(m_allocator)
	, m_debug_triangles(m_allocator)
	, m_debug_lines(m_allocator)
	, m_debug_points(m_allocator)
	, m_temporary_infos(m_allocator)
	, m_active_global_light_gameobject(INVALID_GAMEOBJECT)
	, m_is_grass_enabled(true)
	, m_is_game_running(false)
	, m_particle_emitters(m_allocator)
	, m_scripted_particle_emitters(m_allocator)
	, m_point_lights_map(m_allocator)
	, m_bone_attachments(m_allocator)
	, m_environment_probes(m_allocator)
	, m_lod_multiplier(1.0f)
	, m_time(0)
	, m_is_updating_attachments(false)
{
	m_project.gameobjectTransformed().bind<RenderSceneImpl, &RenderSceneImpl::onGameObjectMoved>(this);
	m_project.gameobjectDestroyed().bind<RenderSceneImpl, &RenderSceneImpl::onGameObjectDestroyed>(this);
	m_culling_system = CullingSystem::create(m_allocator);
	m_model_instances.reserve(5000);

	MaterialManager& manager = m_renderer.getMaterialManager();

	for (auto& i : COMPONENT_INFOS)
	{
		project.registerComponentType(i.type, this, i.creator, i.destroyer, i.serialize, i.deserialize);
	}
}


RenderScene* RenderScene::createInstance(Renderer& renderer,
	Engine& engine,
	Project& project,
	IAllocator& allocator)
{
	return MALMY_NEW(allocator, RenderSceneImpl)(renderer, engine, project, allocator);
}


void RenderScene::destroyInstance(RenderScene* scene)
{
	MALMY_DELETE(scene->getAllocator(), static_cast<RenderSceneImpl*>(scene));
}


void RenderScene::registerLuaAPI(lua_State* L)
{
	Pipeline::registerLuaAPI(L);
	Model::registerLuaAPI(L);

	#define REGISTER_FUNCTION(F)\
		do { \
		auto f = &LuaWrapper::wrapMethod<RenderSceneImpl, decltype(&RenderSceneImpl::F), &RenderSceneImpl::F>; \
		LuaWrapper::createSystemFunction(L, "Renderer", #F, f); \
		} while(false) \

	REGISTER_FUNCTION(setGlobalLODMultiplier);
	REGISTER_FUNCTION(getGlobalLODMultiplier);
	REGISTER_FUNCTION(getCameraViewProjection);
	REGISTER_FUNCTION(getGlobalLightGameObject);
	REGISTER_FUNCTION(getActiveGlobalLight);
	REGISTER_FUNCTION(getCameraInSlot);
	REGISTER_FUNCTION(getCameraSlot);
	REGISTER_FUNCTION(getModelInstanceModel);
	REGISTER_FUNCTION(addDebugCross);
	REGISTER_FUNCTION(addDebugLine);
	REGISTER_FUNCTION(addDebugCircle);
	REGISTER_FUNCTION(addDebugSphere);
	REGISTER_FUNCTION(getTerrainMaterial);
	REGISTER_FUNCTION(getTerrainNormalAt);
	REGISTER_FUNCTION(setTerrainHeightAt);
	REGISTER_FUNCTION(enableModelInstance);
	REGISTER_FUNCTION(getPoseBonePosition);

	#undef REGISTER_FUNCTION

	#define REGISTER_FUNCTION(F)\
		do { \
		auto f = &LuaWrapper::wrap<decltype(&RenderSceneImpl::LUA_##F), &RenderSceneImpl::LUA_##F>; \
		LuaWrapper::createSystemFunction(L, "Renderer", #F, f); \
		} while(false) \

	REGISTER_FUNCTION(createPipeline);
	REGISTER_FUNCTION(destroyPipeline);
	REGISTER_FUNCTION(setPipelineScene);
	REGISTER_FUNCTION(getPipelineScene);
	REGISTER_FUNCTION(pipelineRender);
	REGISTER_FUNCTION(getRenderBuffer);
	REGISTER_FUNCTION(getMaterialTexture);
	REGISTER_FUNCTION(getTextureWidth);
	REGISTER_FUNCTION(getTextureHeight);
	REGISTER_FUNCTION(getTexturePixel);
	REGISTER_FUNCTION(setTexturePixel);
	REGISTER_FUNCTION(getTextureHandle);
	REGISTER_FUNCTION(updateTextureData);
	REGISTER_FUNCTION(setModelInstanceMaterial);
	REGISTER_FUNCTION(setModelInstancePath);
	REGISTER_FUNCTION(getModelBoneIndex);
	REGISTER_FUNCTION(makeScreenshot);
	REGISTER_FUNCTION(compareTGA);
	REGISTER_FUNCTION(getTerrainHeightAt);
	REGISTER_FUNCTION(emitParticle);

	LuaWrapper::createSystemFunction(L, "Renderer", "castCameraRay", &RenderSceneImpl::LUA_castCameraRay);

	#undef REGISTER_FUNCTION
}


} // namespace Malmy

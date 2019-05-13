#include "editor_icon.h"
#include "editor/platform_interface.h"
#include "engine/math_utils.h"
#include "engine/matrix.h"
#include "engine/reflection.h"
#include "engine/resource_manager_base.h"
#include "engine/project/project.h"
#include "render_interface.h"
#include "world_editor.h"
#include <cmath>


namespace Malmy
{


static const ComponentType MODEL_INSTANCE_TYPE = Reflection::getComponentType("renderable");
static const ComponentType PHYSICAL_CONTROLLER_TYPE = Reflection::getComponentType("physical_controller");
static const ComponentType BOX_RIGID_ACTOR_TYPE = Reflection::getComponentType("box_rigid_actor");
static const ComponentType CAMERA_TYPE = Reflection::getComponentType("camera");
static const ComponentType GLOBAL_LIGHT_TYPE = Reflection::getComponentType("global_light");
static const ComponentType POINT_LIGHT_TYPE = Reflection::getComponentType("point_light");
static const ComponentType TERRAIN_TYPE = Reflection::getComponentType("terrain");


enum class IconType
{
	PHYSICAL_CONTROLLER,
	PHYSICAL_BOX,
	CAMERA,
	LIGHT,
	TERRAIN,
	GAMEOBJECT,

	COUNT
};


const char* ICONS[(int)IconType::COUNT] =
{
	"phy_controller_icon",
	"phy_box_icon",
	"camera_icon",
	"directional_light_icon",
	"terrain_icon",
	"icon"
};


static const float ORTHO_SIZE_SCALE = 1 / 20.0f;


struct EditorIconsImpl MALMY_FINAL : public EditorIcons
{
	struct Icon
	{
		GameObject gameobject;
		IconType type;
		float scale;
	};


	explicit EditorIconsImpl(WorldEditor& editor)
		: m_editor(editor)
		, m_icons(editor.getAllocator())
	{
		m_render_interface = nullptr;
		m_icons.reserve(200);
		editor.projectDestroyed().bind<EditorIconsImpl, &EditorIconsImpl::clear>(this);
		editor.projectCreated().bind<EditorIconsImpl, &EditorIconsImpl::onProjectCreated>(this);
		if (m_editor.getProject()) onProjectCreated();
	}


	~EditorIconsImpl()
	{
		m_editor.projectDestroyed().unbind<EditorIconsImpl, &EditorIconsImpl::clear>(this);
		m_editor.projectCreated().unbind<EditorIconsImpl, &EditorIconsImpl::onProjectCreated>(this);
		setRenderInterface(nullptr);

		if(m_editor.getProject())
		{
			auto& project = *m_editor.getProject();
			project.gameobjectCreated().unbind<EditorIconsImpl, &EditorIconsImpl::onGameObjectCreated>(this);
			project.gameobjectDestroyed().unbind<EditorIconsImpl, &EditorIconsImpl::destroyIcon>(this);
			project.componentAdded().unbind<EditorIconsImpl, &EditorIconsImpl::refreshIcon>(this);
			project.componentDestroyed().unbind<EditorIconsImpl, &EditorIconsImpl::refreshIcon>(this);
		}
	}


	void onProjectCreated()
	{
		auto& project = *m_editor.getProject();
		project.gameobjectCreated().bind<EditorIconsImpl, &EditorIconsImpl::onGameObjectCreated>(this);
		project.gameobjectDestroyed().bind<EditorIconsImpl, &EditorIconsImpl::destroyIcon>(this);
		project.componentAdded().bind<EditorIconsImpl, &EditorIconsImpl::refreshIcon>(this);
		project.componentDestroyed().bind<EditorIconsImpl, &EditorIconsImpl::refreshIcon>(this);
	}


	void onGameObjectCreated(GameObject gameobject)
	{
		createIcon(gameobject);
	}


	void destroyIcon(GameObject gameobject)
	{
		for(int i = 0, c = m_icons.size(); i < c; ++i)
		{
			if(m_icons[i].gameobject == gameobject)
			{
				m_icons.eraseFast(i);
				return;
			}
		}
	}


	void refreshIcon(const ComponentUID& cmp)
	{
		destroyIcon(cmp.gameobject);
		createIcon(cmp.gameobject);
	}


	void createIcon(GameObject gameobject)
	{
		if (!gameobject.isValid()) return;
		if (m_editor.getEditCamera().gameobject == gameobject) return;

		Project& project = *m_editor.getProject();
		
		if (project.getComponent(gameobject, MODEL_INSTANCE_TYPE).isValid()) return;

		auto& icon = m_icons.emplace();
		icon.gameobject = gameobject;
		icon.type = IconType::GAMEOBJECT;
		for (ComponentUID cmp = project.getFirstComponent(gameobject); cmp.isValid(); cmp = project.getNextComponent(cmp))
		{
			if(cmp.type == PHYSICAL_CONTROLLER_TYPE)
			{
				icon.type = IconType::PHYSICAL_CONTROLLER;
				break;
			}
			if(cmp.type == BOX_RIGID_ACTOR_TYPE)
			{
				icon.type = IconType::PHYSICAL_BOX;
				break;
			}
			if(cmp.type == CAMERA_TYPE)
			{
				icon.type = IconType::CAMERA;
				break;
			}
			if(cmp.type == GLOBAL_LIGHT_TYPE || cmp.type == POINT_LIGHT_TYPE)
			{
				icon.type = IconType::LIGHT;
				break;
			}
			if(cmp.type == TERRAIN_TYPE)
			{
				icon.type = IconType::TERRAIN;
				break;
			}
		}
	}



	void refresh() override
	{
		clear();
		auto& project = *m_editor.getProject();
		for (GameObject gameobject = project.getFirstGameObject(); gameobject.isValid(); gameobject = project.getNextGameObject(gameobject))
		{
			createIcon(gameobject);
		}
	}


	void clear() override
	{
		m_icons.clear();
	}


	Hit raycast(const Vec3& origin, const Vec3& dir) override
	{
		Hit hit;
		hit.t = -1;
		hit.gameobject = INVALID_GAMEOBJECT;

		auto* render_interface = m_editor.getRenderInterface();
		if(!render_interface) return hit;

		const auto& project = *m_editor.getProject();
		GameObject camera = m_editor.getEditCamera().gameobject;
		if (!camera.isValid()) return hit;
		Matrix camera_mtx = project.getMatrix(m_editor.getEditCamera().gameobject);
		bool is_ortho = render_interface->isCameraOrtho(camera);
		float ortho_size = render_interface->getCameraOrthoSize(camera);

		for(auto& icon : m_icons)
		{
			Matrix icon_matrix = getIconMatrix(icon, camera_mtx, is_ortho, ortho_size);
			
			float t = m_editor.getRenderInterface()->castRay(m_models[(int)icon.type], origin, dir, icon_matrix, nullptr);
			if(t >= 0 && (t < hit.t || hit.t < 0))
			{
				hit.t = t;
				hit.gameobject = icon.gameobject;
			}
		}

		return hit;
	}


	void setRenderInterface(RenderInterface* render_interface) override
	{
		if (m_render_interface)
		{
			for (auto& model : m_models)
			{
				m_render_interface->unloadModel(model);
			}
		}
		m_render_interface = render_interface;
		if (m_render_interface)
		{
			for (int i = 0; i < lengthOf(ICONS); ++i)
			{
				StaticString<MAX_PATH_LENGTH> tmp("models/editor/", ICONS[i], "_3d.msh");
				m_is_3d[i] = PlatformInterface::fileExists(tmp);
				if (m_is_3d[i])
				{
					Path path(tmp);
					m_models[i] = m_render_interface->loadModel(path);
				}
				else
				{
					tmp.data[0] = '\0';
					tmp << "models/editor/" << ICONS[i] << ".msh";
					Path path(tmp);
					m_models[i] = m_render_interface->loadModel(path);
				}
			}
		}
	}


	Matrix getIconMatrix(const Icon& icon, const Matrix& camera_matrix, bool is_ortho, float ortho_size) const
	{
		Matrix ret;
		if (m_is_3d[(int)icon.type])
		{
			ret = m_editor.getProject()->getMatrix(icon.gameobject);
		}
		else
		{
			ret = camera_matrix;
			ret.setTranslation(m_editor.getProject()->getPosition(icon.gameobject));
		}
		if (is_ortho)
		{
			ret.multiply3x3(ortho_size * ORTHO_SIZE_SCALE);
		}
		else
		{
			ret.multiply3x3(icon.scale > 0 ? icon.scale : 1);
		}
		return ret;
	}


	void render() override
	{
		static const float MIN_SCALE_FACTOR = 10;
		static const float MAX_SCALE_FACTOR = 60;

		auto* render_interface = m_editor.getRenderInterface();
		if(!render_interface) return;

		const auto& project = *m_editor.getProject();
		GameObject camera = m_editor.getEditCamera().gameobject;
		if (!camera.isValid()) return;
		Matrix camera_mtx = project.getMatrix(m_editor.getEditCamera().gameobject);
		Vec3 camera_pos = camera_mtx.getTranslation();
		float fov = m_editor.getRenderInterface()->getCameraFOV(camera);
		bool is_ortho = m_editor.getRenderInterface()->isCameraOrtho(camera);
		float ortho_size = is_ortho ? m_editor.getRenderInterface()->getCameraOrthoSize(camera) : 1;

		for(auto& icon : m_icons)
		{
			Vec3 position = project.getPosition(icon.gameobject);
			float distance = (position - camera_pos).length();
			float scale_factor = MIN_SCALE_FACTOR + distance;
			scale_factor = Math::clamp(scale_factor, MIN_SCALE_FACTOR, MAX_SCALE_FACTOR);
			icon.scale = tan(fov * 0.5f) * distance / scale_factor;
			
			Matrix icon_mtx = getIconMatrix(icon, camera_mtx, is_ortho, ortho_size);
			render_interface->renderModel(m_models[(int)icon.type], icon_mtx);
		}
	}

	Array<Icon> m_icons;
	RenderInterface::ModelHandle m_models[(int)IconType::COUNT];
	bool m_is_3d[(int)IconType::COUNT];
	WorldEditor& m_editor;
	RenderInterface* m_render_interface;
};


EditorIcons* EditorIcons::create(WorldEditor& editor)
{
	return MALMY_NEW(editor.getAllocator(), EditorIconsImpl)(editor);
}


void EditorIcons::destroy(EditorIcons& icons)
{
	auto& i = static_cast<EditorIconsImpl&>(icons);
	MALMY_DELETE(i.m_editor.getAllocator(), &icons);
}


} // namespace Malmy

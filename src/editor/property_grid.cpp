#include "property_grid.h"
#include "asset_browser.h"
#include "editor/prefab_system.h"
#include "editor/studio_app.h"
#include "editor/world_editor.h"
#include "engine/blob.h"
#include "engine/crc32.h"
#include "engine/iplugin.h"
#include "engine/math_utils.h"
#include "engine/prefab.h"
#include "engine/reflection.h"
#include "engine/resource.h"
#include "engine/serializer.h"
#include "engine/project/project.h"
#include "engine/vec.h"
#include "imgui/imgui.h"
#include "utils.h"
#include <cmath>
#include <cstdlib>


namespace Malmy
{


PropertyGrid::PropertyGrid(StudioApp& app)
	: m_app(app)
	, m_is_open(true)
	, m_editor(app.getWorldEditor())
	, m_plugins(app.getWorldEditor().getAllocator())
	, m_deferred_select(INVALID_GAMEOBJECT)
{
	m_particle_emitter_updating = true;
	m_particle_emitter_timescale = 1.0f;
	m_component_filter[0] = '\0';
}


PropertyGrid::~PropertyGrid()
{
	ASSERT(m_plugins.empty());
}


struct GridUIVisitor MALMY_FINAL : Reflection::IPropertyVisitor
{
	GridUIVisitor(StudioApp& app, int index, const Array<GameObject>& entities, ComponentType cmp_type, WorldEditor& editor)
		: m_entities(entities)
		, m_cmp_type(cmp_type)
		, m_editor(editor)
		, m_index(index)
		, m_grid(app.getPropertyGrid())
		, m_app(app)
	{}


	ComponentUID getComponent() const
	{
		ComponentUID first_gameobject_cmp;
		first_gameobject_cmp.type = m_cmp_type;
		first_gameobject_cmp.scene = m_editor.getProject()->getScene(m_cmp_type);
		first_gameobject_cmp.gameobject = m_entities[0];
		return first_gameobject_cmp;
	}


	struct Attributes : Reflection::IAttributeVisitor
	{
		void visit(const Reflection::IAttribute& attr) override
		{
			switch (attr.getType())
			{
				case Reflection::IAttribute::RADIANS:
					is_radians = true;
					break;
				case Reflection::IAttribute::COLOR:
					is_color = true;
					break;
				case Reflection::IAttribute::MIN:
					min = ((Reflection::MinAttribute&)attr).min;
					break;
				case Reflection::IAttribute::CLAMP:
					min = ((Reflection::ClampAttribute&)attr).min;
					max = ((Reflection::ClampAttribute&)attr).max;
					break;
				case Reflection::IAttribute::RESOURCE:
					resource_type = ((Reflection::ResourceAttribute&)attr).type;
					break;
			}
		}

		float max = FLT_MAX;
		float min = -FLT_MAX;
		bool is_color = false;
		bool is_radians = false;
		ResourceType resource_type;
	};


	static Attributes getAttributes(const Reflection::PropertyBase& prop)
	{
		Attributes attrs;
		prop.visit(attrs);
		return attrs;
	}


	bool skipProperty(const Reflection::PropertyBase& prop)
	{
		return equalStrings(prop.name, "Enabled");
	}


	void visit(const Reflection::Property<float>& prop) override
	{
		if (skipProperty(prop)) return;
		Attributes attrs = getAttributes(prop);
		ComponentUID cmp = getComponent();
		float f;
		OutputBlob blob(&f, sizeof(f));
		prop.getValue(cmp, m_index, blob);

		if (attrs.is_radians) f = Math::radiansToDegrees(f);
		if (ImGui::DragFloat(prop.name, &f, 1, attrs.min, attrs.max))
		{
			f = Math::clamp(f, attrs.min, attrs.max);
			if (attrs.is_radians) f = Math::degreesToRadians(f);
			m_editor.setProperty(m_cmp_type, m_index, prop, &m_entities[0], m_entities.size(), &f, sizeof(f));
		}
	}


	void visit(const Reflection::Property<int>& prop) override
	{
		if (skipProperty(prop)) return;
		ComponentUID cmp = getComponent();
		int value;
		OutputBlob blob(&value, sizeof(value));
		prop.getValue(cmp, m_index, blob);

		if (ImGui::InputInt(prop.name, &value))
		{
			m_editor.setProperty(m_cmp_type, m_index, prop, &m_entities[0], m_entities.size(), &value, sizeof(value));
		}
	}


	void visit(const Reflection::Property<GameObject>& prop) override
	{
		ComponentUID cmp = getComponent();
		GameObject gameobject;
		OutputBlob blob(&gameobject, sizeof(gameobject));
		prop.getValue(cmp, m_index, blob);

		char buf[128];
		getGameObjectListDisplayName(m_editor, buf, lengthOf(buf), gameobject);
		ImGui::PushID(prop.name);
		
		float item_w = ImGui::CalcItemWidth();
		auto& style = ImGui::GetStyle();
		float text_width = Math::maximum(50.0f, item_w - ImGui::CalcTextSize("...").x - style.FramePadding.x * 2);

		auto pos = ImGui::GetCursorPos();
		pos.x += text_width;
		ImGui::BeginGroup();
		ImGui::AlignTextToFramePadding();
		ImGui::PushTextWrapPos(pos.x);
		ImGui::Text("%s", buf);
		ImGui::PopTextWrapPos();
		ImGui::SameLine();
		ImGui::SetCursorPos(pos);
		if (ImGui::Button("..."))
		{
			ImGui::OpenPopup(prop.name);
		}
		ImGui::EndGroup();
		ImGui::SameLine();
		ImGui::Text("%s", prop.name);

		Project& project = *m_editor.getProject();
		if (ImGui::BeginPopup(prop.name))
		{
			if (gameobject.isValid() && ImGui::Button("Select")) m_grid.m_deferred_select = gameobject;

			static char gameobject_filter[32] = {};
			ImGui::LabellessInputText("Filter", gameobject_filter, sizeof(gameobject_filter));
			for (auto i = project.getFirstGameObject(); i.isValid(); i = project.getNextGameObject(i))
			{
				getGameObjectListDisplayName(m_editor, buf, lengthOf(buf), i);
				bool show = gameobject_filter[0] == '\0' || stristr(buf, gameobject_filter) != 0;
				if (show && ImGui::Selectable(buf))
				{
					m_editor.setProperty(m_cmp_type, m_index, prop, &m_entities[0], m_entities.size(), &i, sizeof(i));
				}
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();
	}


	void visit(const Reflection::Property<Int2>& prop) override
	{
		if (skipProperty(prop)) return;
		ComponentUID cmp = getComponent();
		Int2 value;
		OutputBlob blob(&value, sizeof(value));
		prop.getValue(cmp, m_index, blob);
		if (ImGui::DragInt2(prop.name, &value.x))
		{
			m_editor.setProperty(m_cmp_type, m_index, prop, &m_entities[0], m_entities.size(), &value, sizeof(value));
		}
	}


	void visit(const Reflection::Property<Vec2>& prop) override
	{
		if (skipProperty(prop)) return;
		ComponentUID cmp = getComponent();
		Vec2 value;
		OutputBlob blob(&value, sizeof(value));
		prop.getValue(cmp, m_index, blob);
		if (ImGui::DragFloat2(prop.name, &value.x))
		{
			m_editor.setProperty(m_cmp_type, m_index, prop, &m_entities[0], m_entities.size(), &value, sizeof(value));
		}
	}


	void visit(const Reflection::Property<Vec3>& prop) override
	{
		if (skipProperty(prop)) return;
		Attributes attrs = getAttributes(prop);
		ComponentUID cmp = getComponent();
		Vec3 value;
		OutputBlob blob(&value, sizeof(value));
		prop.getValue(cmp, m_index, blob);

		if (attrs.is_color)
		{
			if (ImGui::ColorEdit3(prop.name, &value.x))
			{
				m_editor.setProperty(m_cmp_type, m_index, prop, &m_entities[0], m_entities.size(), &value, sizeof(value));
			}
		}
		else
		{
			if (attrs.is_radians) value = Math::radiansToDegrees(value);
			if (ImGui::DragFloat3(prop.name, &value.x, 1, attrs.min, attrs.max))
			{
				if (attrs.is_radians) value = Math::degreesToRadians(value);
				m_editor.setProperty(m_cmp_type, m_index, prop, &m_entities[0], m_entities.size(), &value, sizeof(value));
			}
		}
	}


	void visit(const Reflection::Property<Vec4>& prop) override
	{
		if (skipProperty(prop)) return;
		Attributes attrs = getAttributes(prop);
		ComponentUID cmp = getComponent();
		Vec4 value;
		OutputBlob blob(&value, sizeof(value));
		prop.getValue(cmp, m_index, blob);

		if (attrs.is_color)
		{
			if (ImGui::ColorEdit4(prop.name, &value.x))
			{
				m_editor.setProperty(m_cmp_type, m_index, prop, &m_entities[0], m_entities.size(), &value, sizeof(value));
			}
		}
		else
		{
			if (ImGui::DragFloat4(prop.name, &value.x))
			{
				m_editor.setProperty(m_cmp_type, m_index, prop, &m_entities[0], m_entities.size(), &value, sizeof(value));
			}
		}
	}


	void visit(const Reflection::Property<bool>& prop) override
	{
		if (skipProperty(prop)) return;
		ComponentUID cmp = getComponent();
		bool value;
		OutputBlob blob(&value, sizeof(value));
		prop.getValue(cmp, m_index, blob);

		if (ImGui::CheckboxEx(prop.name, &value))
		{
			m_editor.setProperty(m_cmp_type, m_index, prop, &m_entities[0], m_entities.size(), &value, sizeof(value));
		}
	}


	void visit(const Reflection::Property<Path>& prop) override
	{
		if (skipProperty(prop)) return;
		ComponentUID cmp = getComponent();
		char tmp[1024];
		OutputBlob blob(&tmp, sizeof(tmp));
		prop.getValue(cmp, m_index, blob);

		Attributes attrs = getAttributes(prop);

		if (attrs.resource_type != INVALID_RESOURCE_TYPE)
		{
			if (m_app.getAssetBrowser().resourceInput(prop.name, StaticString<20>("", (u64)&prop), tmp, sizeof(tmp), attrs.resource_type))
			{
				m_editor.setProperty(m_cmp_type, m_index, prop, &m_entities[0], m_entities.size(), tmp, stringLength(tmp) + 1);
			}
		}
		else
		{
			if (ImGui::InputText(prop.name, tmp, sizeof(tmp)))
			{
				m_editor.setProperty(m_cmp_type, m_index, prop, &m_entities[0], m_entities.size(), tmp, stringLength(tmp) + 1);
			}
		}
	}


	void visit(const Reflection::Property<const char*>& prop) override
	{
		if (skipProperty(prop)) return;
		ComponentUID cmp = getComponent();
		char tmp[1024];
		OutputBlob blob(&tmp, sizeof(tmp));
		prop.getValue(cmp, m_index, blob);

		if (ImGui::InputText(prop.name, tmp, sizeof(tmp)))
		{
			m_editor.setProperty(m_cmp_type, m_index, prop, &m_entities[0], m_entities.size(), tmp, stringLength(tmp) + 1);
		}
	}


	void visit(const Reflection::IBlobProperty& prop) override {}


	void visit(const Reflection::ISampledFuncProperty& prop) override
	{
		if (skipProperty(prop)) return;
		static const int MIN_COUNT = 6;
		ComponentUID cmp = getComponent();

		struct Point
		{
			Vec2 prev_tangent;
			Vec2 p;
			Vec2 next_tangent;
		};

		OutputBlob blob(m_editor.getAllocator());
		prop.getValue(cmp, -1, blob);
		blob.reserve(blob.getPos() + sizeof(Point));
		int count;
		InputBlob input(blob);
		input.read(count);
		count /= 3;
		Point* points = (Point*)input.skip(sizeof(Point) * count);

		bool changed = false;
		int new_count;
		int changed_idx = ImGui::CurveEditor(prop.name, (float*)points, count, ImVec2(-1, -1), 0, &new_count);
		if (changed_idx >= 0)
		{
			changed = true;
			points[changed_idx].p.x = Math::clamp(points[changed_idx].p.x, 0.0f, 1.0f);
			points[changed_idx].p.y = Math::clamp(points[changed_idx].p.y, 0.0f, 1.0f);
		}
		if (new_count != count)
		{
			changed = true;
			if (new_count > count)
				blob.resize(blob.getPos() + sizeof(Point));
			else
				blob.resize(blob.getPos() - sizeof(Point));
			count = new_count;
			*(int*)blob.getData() = count * 3;
		}
		
		if (changed)
		{
			for (int i = 1; i < count; ++i)
			{
				auto prev_p = points[i-1].p;
				auto next_p = points[i].p;
				auto& tangent = points[i - 1].next_tangent;
				auto& tangent2 = points[i].prev_tangent;
				float half = 0.5f * (next_p.x - prev_p.x);
				tangent = tangent.normalized() * half;
				tangent2 = tangent2.normalized() * half;
			}
			points[0].p.x = 0;
			points[count - 1].p.x = prop.getMaxX();
			m_editor.setProperty(cmp.type, -1, prop, &m_entities[0], m_entities.size(), blob.getData(), blob.getPos());
		}
	}

	void visit(const Reflection::IArrayProperty& prop) override
	{
		if (skipProperty(prop)) return;
		ImGui::Unindent();
		bool is_open = ImGui::TreeNodeEx(prop.name, ImGuiTreeNodeFlags_AllowItemOverlap);
		if (m_entities.size() > 1)
		{
			ImGui::Text("Multi-object editing not supported.");
			if (is_open) ImGui::TreePop();
			ImGui::Indent();
			return;
		}

		ComponentUID cmp = getComponent();
		int count = prop.getCount(cmp);
		const ImGuiStyle& style = ImGui::GetStyle();
		if (prop.canAddRemove())
		{
			ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize("Add").x - style.FramePadding.x * 2 - style.WindowPadding.x - 15);
			if (ImGui::SmallButton("Add"))
			{
				m_editor.addArrayPropertyItem(cmp, prop);
				count = prop.getCount(cmp);
			}
		}
		if (!is_open)
		{
			ImGui::Indent();
			return;
		}

		for (int i = 0; i < count; ++i)
		{
			char tmp[10];
			toCString(i, tmp, sizeof(tmp));
			ImGui::PushID(i);
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap;
			bool is_open = !prop.canAddRemove() || ImGui::TreeNodeEx(tmp, flags);
			if (prop.canAddRemove())
			{
				ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize("Remove").x - style.FramePadding.x * 2 - style.WindowPadding.x - 15);
				if (ImGui::SmallButton("Remove"))
				{
					m_editor.removeArrayPropertyItem(cmp, i, prop);
					--i;
					count = prop.getCount(cmp);
					if(is_open) ImGui::TreePop();
					ImGui::PopID();
					continue;
				}
			}

			if (is_open)
			{
				GridUIVisitor v(m_app, i, m_entities, m_cmp_type, m_editor);
				prop.visit(v);
				if (prop.canAddRemove()) ImGui::TreePop();
			}

			ImGui::PopID();
		}
		ImGui::TreePop();
		ImGui::Indent();
	}


	void visit(const Reflection::IEnumProperty& prop) override
	{
		if (skipProperty(prop)) return;
		if (m_entities.size() > 1)
		{
			ImGui::LabelText(prop.name, "Multi-object editing not supported.");
			return;
		}

		ComponentUID cmp = getComponent();
		int value;
		OutputBlob blob(&value, sizeof(value));
		prop.getValue(cmp, m_index, blob);
		int count = prop.getEnumCount(cmp);

		struct Data
		{
			const Reflection::IEnumProperty* prop;
			ComponentUID cmp;
		};

		auto getter = [](void* data, int index, const char** out) -> bool {
			Data* combo_data = (Data*)data;
			*out = combo_data->prop->getEnumName(combo_data->cmp, index);
			return true;
		};

		Data data;
		data.cmp = cmp;
		data.prop = &prop;

		int idx = prop.getEnumValueIndex(cmp, value);
		if (ImGui::Combo(prop.name, &idx, getter, &data, count))
		{
			value = prop.getEnumValue(cmp, idx);
			m_editor.setProperty(cmp.type, m_index, prop, &cmp.gameobject, 1, &value, sizeof(value));
		}
	}


	StudioApp& m_app;
	WorldEditor& m_editor;
	ComponentType m_cmp_type;
	const Array<GameObject>& m_entities;
	int m_index;
	PropertyGrid& m_grid;
};


static bool componentTreeNode(StudioApp& app, ComponentType cmp_type, const GameObject* entities, int entities_count)
{
	static const u32 ENABLED_HASH = crc32("Enabled");
	const Reflection::PropertyBase* enabled_prop = Reflection::getProperty(cmp_type, ENABLED_HASH);

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap;
	ImGui::Separator();
	const char* cmp_type_name = app.getComponentTypeName(cmp_type);
	ImGui::PushFont(app.getBoldFont());
	bool is_open;
	if (enabled_prop)
	{
		is_open = ImGui::TreeNodeEx((void*)(uintptr)cmp_type.index, flags, "%s", "");
		ImGui::SameLine();
		bool b;
		ComponentUID cmp;
		cmp.type = cmp_type;
		cmp.gameobject = entities[0];
		cmp.scene = app.getWorldEditor().getProject()->getScene(cmp_type);
		OutputBlob blob(&b, sizeof(b));
		enabled_prop->getValue(cmp, -1, blob);
		if(ImGui::Checkbox(cmp_type_name, &b))
		{
			app.getWorldEditor().setProperty(cmp_type, -1, *enabled_prop, entities, entities_count, &b, sizeof(b));
		}
	}
	else
	{ 
		is_open = ImGui::TreeNodeEx((void*)(uintptr)cmp_type.index, flags, "%s", cmp_type_name);
	}
	ImGui::PopFont();
	return is_open;
}


void PropertyGrid::showComponentProperties(const Array<GameObject>& entities, ComponentType cmp_type)
{
	bool is_open = componentTreeNode(m_app, cmp_type, &entities[0], entities.size());
	ImGuiStyle& style = ImGui::GetStyle();
	ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize("Remove").x - style.FramePadding.x * 2 - style.WindowPadding.x - 15);
	if (ImGui::SmallButton("Remove"))
	{
		m_editor.destroyComponent(&entities[0], entities.size(), cmp_type);
		if (is_open) ImGui::TreePop();
		return;
	}

	if (!is_open) return;

	const Reflection::ComponentBase* component = Reflection::getComponent(cmp_type);
	GridUIVisitor visitor(m_app, -1, entities, cmp_type, m_editor);
	if (component) component->visit(visitor);

	if (m_deferred_select.isValid())
	{
		m_editor.selectEntities(&m_deferred_select, 1, false);
		m_deferred_select = INVALID_GAMEOBJECT;
	}

	if (entities.size() == 1)
	{
		ComponentUID cmp;
		cmp.type = cmp_type;
		cmp.scene = m_editor.getProject()->getScene(cmp.type);
		cmp.gameobject = entities[0];
		for (auto* i : m_plugins)
		{
			i->onGUI(*this, cmp);
		}
	}
	ImGui::TreePop();
}


bool PropertyGrid::gameobjectInput(const char* label, const char* str_id, GameObject& gameobject)
{
	const auto& style = ImGui::GetStyle();
	float item_w = ImGui::CalcItemWidth();
	ImGui::PushItemWidth(
		item_w - ImGui::CalcTextSize("...").x - style.FramePadding.x * 2 - style.ItemSpacing.x);
	char buf[50];
	getGameObjectListDisplayName(m_editor, buf, sizeof(buf), gameobject);
	ImGui::LabelText("", "%s", buf);
	ImGui::SameLine();
	StaticString<30> popup_name("pu", str_id);
	if (ImGui::Button(StaticString<30>("...###br", str_id)))
	{
		ImGui::OpenPopup(popup_name);
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (auto* payload = ImGui::AcceptDragDropPayload("gameobject"))
		{
			gameobject = *(GameObject*)payload->Data;
			ImGui::EndDragDropTarget();
			return true;
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::SameLine();
	ImGui::Text("%s", label);
	ImGui::PopItemWidth();

	if (ImGui::BeginPopup(popup_name))
	{
		if (gameobject.isValid())
		{
			if (ImGui::Button("Select current")) m_deferred_select = gameobject;
			ImGui::SameLine();
			if (ImGui::Button("Empty"))
			{
				gameobject = INVALID_GAMEOBJECT;
				ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
				return true;
			}
		}
		Project* project = m_editor.getProject();
		static char gameobject_filter[32] = {};
		ImGui::LabellessInputText("Filter", gameobject_filter, sizeof(gameobject_filter));
		if (ImGui::ListBoxHeader("Entities"))
		{
			if (gameobject_filter[0])
			{
				for (auto i = project->getFirstGameObject(); i.isValid(); i = project->getNextGameObject(i))
				{
					getGameObjectListDisplayName(m_editor, buf, lengthOf(buf), i);
					if (stristr(buf, gameobject_filter) == nullptr) continue;
					if (ImGui::Selectable(buf))
					{
						ImGui::ListBoxFooter();
						gameobject = i;
						ImGui::CloseCurrentPopup();
						ImGui::EndPopup();
						return true;
					}
				}
			}
			else
			{
				for (auto i = project->getFirstGameObject(); i.isValid(); i = project->getNextGameObject(i))
				{
					getGameObjectListDisplayName(m_editor, buf, lengthOf(buf), i);
					if (ImGui::Selectable(buf))
					{
						ImGui::ListBoxFooter();
						gameobject = i;
						ImGui::CloseCurrentPopup();
						ImGui::EndPopup();
						return true;
					}
				}
			}
			ImGui::ListBoxFooter();
		}

		ImGui::EndPopup();
	}
	return false;
}


void PropertyGrid::showCoreProperties(const Array<GameObject>& entities) const
{
	char name[256];
	const char* tmp = m_editor.getProject()->getGameObjectName(entities[0]);
	copyString(name, tmp);
	if (ImGui::LabellessInputText("Name", name, sizeof(name))) m_editor.setGameObjectName(entities[0], name);
	ImGui::PushFont(m_app.getBoldFont());
	if (!ImGui::TreeNodeEx("General", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::PopFont();
		return;
	}
	ImGui::PopFont();
	if (entities.size() == 1)
	{
		PrefabSystem& prefab_system = m_editor.getPrefabSystem();
		PrefabResource* prefab = prefab_system.getPrefabResource(entities[0]);
		if (prefab)
		{
			ImGui::SameLine();
			if (ImGui::Button("Save prefab"))
			{
				prefab_system.savePrefab(prefab->getPath());
			}
		}

		GameObjectGUID guid = m_editor.getGameObjectGUID(entities[0]);
		if (guid == INVALID_GAMEOBJECT_GUID)
		{
			ImGui::Text("ID: %d, GUID: runtime", entities[0].index);
		}
		else
		{
			char guid_str[32];
			toCString(guid.value, guid_str, lengthOf(guid_str));
			ImGui::Text("ID: %d, GUID: %s", entities[0].index, guid_str);
		}

		GameObject parent = m_editor.getProject()->getParent(entities[0]);
		if (parent.isValid())
		{
			getGameObjectListDisplayName(m_editor, name, lengthOf(name), parent);
			ImGui::LabelText("Parent", "%s", name);

			Transform tr = m_editor.getProject()->getLocalTransform(entities[0]);
			Vec3 old_pos = tr.pos;
			if (ImGui::DragFloat3("Local position", &tr.pos.x))
			{
				WorldEditor::Coordinate coord = WorldEditor::Coordinate::NONE;
				if (tr.pos.x != old_pos.x) coord = WorldEditor::Coordinate::X;
				if (tr.pos.y != old_pos.y) coord = WorldEditor::Coordinate::Y;
				if (tr.pos.z != old_pos.z) coord = WorldEditor::Coordinate::Z;
				if (coord != WorldEditor::Coordinate::NONE)
				{
					m_editor.setEntitiesLocalCoordinate(&entities[0], entities.size(), (&tr.pos.x)[(int)coord], coord);
				}
			}
		}
	}
	else
	{
		ImGui::LabelText("ID", "%s", "Multiple objects");
		ImGui::LabelText("Name", "%s", "Multi-object editing not supported.");
	}


	Vec3 pos = m_editor.getProject()->getPosition(entities[0]);
	Vec3 old_pos = pos;
	if (ImGui::DragFloat3("Position", &pos.x))
	{
		WorldEditor::Coordinate coord = WorldEditor::Coordinate::NONE;
		if (pos.x != old_pos.x) coord = WorldEditor::Coordinate::X;
		if (pos.y != old_pos.y) coord = WorldEditor::Coordinate::Y;
		if (pos.z != old_pos.z) coord = WorldEditor::Coordinate::Z;
		if (coord != WorldEditor::Coordinate::NONE)
		{
			m_editor.setEntitiesCoordinate(&entities[0], entities.size(), (&pos.x)[(int)coord], coord);
		}
	}

	Project* project = m_editor.getProject();
	Quat rot = project->getRotation(entities[0]);
	Vec3 old_euler = rot.toEuler();
	Vec3 euler = Math::radiansToDegrees(old_euler);
	if (ImGui::DragFloat3("Rotation", &euler.x))
	{
		if (euler.x <= -90.0f || euler.x >= 90.0f) euler.y = 0;
		euler.x = Math::degreesToRadians(Math::clamp(euler.x, -90.0f, 90.0f));
		euler.y = Math::degreesToRadians(fmodf(euler.y + 180, 360.0f) - 180);
		euler.z = Math::degreesToRadians(fmodf(euler.z + 180, 360.0f) - 180);
		rot.fromEuler(euler);
		
		Array<Quat> rots(m_editor.getAllocator());
		for (GameObject gameobject : entities)
		{
			Vec3 tmp = project->getRotation(gameobject).toEuler();
			
			if (fabs(euler.x - old_euler.x) > 0.01f) tmp.x = euler.x;
			if (fabs(euler.y - old_euler.y) > 0.01f) tmp.y = euler.y;
			if (fabs(euler.z - old_euler.z) > 0.01f) tmp.z = euler.z;
			rots.emplace().fromEuler(tmp);
		}
		m_editor.setEntitiesRotations(&entities[0], &rots[0], entities.size());
	}

	float scale = m_editor.getProject()->getScale(entities[0]);
	if (ImGui::DragFloat("Scale", &scale, 0.1f))
	{
		m_editor.setEntitiesScale(&entities[0], entities.size(), scale);
	}
	ImGui::TreePop();
}


static void showAddComponentNode(const StudioApp::AddCmpTreeNode* node, const char* filter)
{
	if (!node) return;

	if (filter[0])
	{
		if (!node->plugin) showAddComponentNode(node->child, filter);
		else if (stristr(node->plugin->getLabel(), filter)) node->plugin->onGUI(false, true);
		showAddComponentNode(node->next, filter);
		return;
	}

	if (node->plugin)
	{
		node->plugin->onGUI(false, false);
		showAddComponentNode(node->next, filter);
		return;
	}

	const char* last = reverseFind(node->label, nullptr, '/');
	if (ImGui::BeginMenu(last ? last + 1 : node->label))
	{
		showAddComponentNode(node->child, filter);
		ImGui::EndMenu();
	}
	showAddComponentNode(node->next, filter);
}


void PropertyGrid::onGUI()
{
	auto& ents = m_editor.getSelectedEntities();
	if (ImGui::BeginDock("Properties", &m_is_open) && !ents.empty())
	{
		if (ImGui::Button("Add component"))
		{
			ImGui::OpenPopup("AddComponentPopup");
		}
		if (ImGui::BeginPopup("AddComponentPopup"))
		{
			ImGui::LabellessInputText("Filter", m_component_filter, sizeof(m_component_filter));
			showAddComponentNode(m_app.getAddComponentTreeRoot().child, m_component_filter);
			ImGui::EndPopup();
		}

		showCoreProperties(ents);

		Project& project = *m_editor.getProject();
		for (ComponentUID cmp = project.getFirstComponent(ents[0]); cmp.isValid();
			 cmp = project.getNextComponent(cmp))
		{
			showComponentProperties(ents, cmp.type);
		}
	}
	ImGui::EndDock();
}


} // namespace Malmy

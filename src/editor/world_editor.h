#pragma once

#include "engine/malmy.h"
#include "engine/array.h"
#include "engine/delegate_list.h"
#include "engine/project/component.h"
#include "engine/vec.h"


namespace Malmy
{


namespace Reflection
{
struct PropertyBase;
struct IArrayProperty;
}

class Engine;
struct GameObjectGUID;
class PrefabSystem;
class Hierarchy;
class InputBlob;
struct IPlugin;
struct ISerializer;
class PropertyDescriptorBase;
class OutputBlob;
class Path;
class Pipeline;
class RenderInterface;
struct Quat;
struct RayCastModelHit;
class Project;

struct MouseButton
{
	enum Value
	{
		LEFT = 1,
		MIDDLE = 2,
		RIGHT = 3
	};
};


class MALMY_EDITOR_API WorldEditor
{
public:
	typedef Array<ComponentUID> ComponentList;
	typedef struct IEditorCommand* (*EditorCommandCreator)(WorldEditor&);

	enum class Coordinate : int
	{
		X,
		Y,
		Z,
		NONE
	};

	struct RayHit
	{
		bool is_hit;
		float t;
		GameObject gameobject;
		Vec3 pos;
	};

	struct MALMY_EDITOR_API Plugin
	{
		virtual ~Plugin() {}

		virtual bool onMouseDown(const RayHit& /*hit*/, int /*x*/, int /*y*/) { return false; }
		virtual void onMouseUp(int /*x*/, int /*y*/, MouseButton::Value /*button*/) {}
		virtual void onMouseMove(int /*x*/, int /*y*/, int /*rel_x*/, int /*rel_y*/) {}
		virtual bool showGizmo(ComponentUID /*cmp*/) { return false; }
	};

public:
	static WorldEditor* create(const char* base_path, Engine& engine, IAllocator& allocator);
	static void destroy(WorldEditor* editor, IAllocator& allocator);

	virtual void setRenderInterface(RenderInterface* interface) = 0;
	virtual RenderInterface* getRenderInterface() = 0;
	virtual void update() = 0;
	virtual void updateEngine() = 0;
	virtual void beginCommandGroup(u32 type) = 0;
	virtual void endCommandGroup() = 0;
	virtual void executeCommand(IEditorCommand* command) = 0;
	virtual IEditorCommand* createEditorCommand(u32 command_type) = 0;
	virtual Engine& getEngine() = 0;
	virtual Project* getProject() = 0;
	virtual IAllocator& getAllocator() = 0;
	virtual void renderIcons() = 0;
	virtual ComponentUID getEditCamera() = 0;
	virtual class Gizmo& getGizmo() = 0;
	virtual bool canUndo() const = 0;
	virtual bool canRedo() const = 0;
	virtual void undo() = 0;
	virtual void redo() = 0;
	virtual void loadProject(const char* basename) = 0;
	virtual void saveProject(const char* basename, bool save_path) = 0;
	virtual void newProject() = 0;
	virtual void copyEntities(const GameObject* entities, int count, ISerializer& serializer) = 0;
	virtual void copyEntities() = 0;
	virtual bool canPasteEntities() const = 0;
	virtual void pasteEntities() = 0;
    virtual void duplicateEntities() = 0;
	virtual void addComponent(ComponentType type) = 0;
	virtual void cloneComponent(const ComponentUID& src, GameObject gameobject) = 0;
	virtual void destroyComponent(const GameObject* entities, int count, ComponentType cmp_type) = 0;
	virtual void createGameObjectGUID(GameObject gameobject) = 0;
	virtual void destroyGameObjectGUID(GameObject gameobject) = 0;
	virtual GameObjectGUID getGameObjectGUID(GameObject gameobject) = 0;
	virtual GameObject addGameObject() = 0;
	virtual void destroyEntities(const GameObject* entities, int count) = 0;
	virtual void selectEntities(const GameObject* entities, int count, bool toggle) = 0;
	virtual GameObject addGameObjectAt(int camera_x, int camera_y) = 0;
	virtual void setEntitiesPositions(const GameObject* entities, const Vec3* positions, int count) = 0;
	virtual void setEntitiesCoordinate(const GameObject* entities, int count, float value, Coordinate coord) = 0;
	virtual void setEntitiesLocalCoordinate(const GameObject* entities, int count, float value, Coordinate coord) = 0;
	virtual void setEntitiesScale(const GameObject* entities, int count, float scale) = 0;
	virtual void setEntitiesScales(const GameObject* entities, const float* scales, int count) = 0;
	virtual void setEntitiesRotations(const GameObject* gameobject, const Quat* rotations, int count) = 0;
	virtual void setEntitiesPositionsAndRotations(const GameObject* gameobject,
		const Vec3* position,
		const Quat* rotation,
		int count) = 0;
	virtual void setGameObjectName(GameObject gameobject, const char* name) = 0;
	virtual void snapDown() = 0;
	virtual void toggleGameMode() = 0;
	virtual void navigate(float forward, float right, float up, float speed) = 0;
	virtual void setProperty(ComponentType component,
		int index,
		const Reflection::PropertyBase& property,
		const GameObject* entities,
		int count,
		const void* data,
		int size) = 0;
	virtual void setCustomPivot() = 0;
	virtual void setSnapMode(bool enable, bool vertex_snap) = 0;
	virtual void setToggleSelection(bool is_toggle) = 0;
	virtual void addArrayPropertyItem(const ComponentUID& cmp, const Reflection::IArrayProperty& property) = 0;
	virtual void removeArrayPropertyItem(const ComponentUID& cmp, int index, const Reflection::IArrayProperty& property) = 0;
	virtual bool isMouseDown(MouseButton::Value button) const = 0;
	virtual bool isMouseClick(MouseButton::Value button) const = 0;
	virtual void inputFrame() = 0;
	virtual void onMouseDown(int x, int y, MouseButton::Value button) = 0;
	virtual void onMouseMove(int x, int y, int relx, int rely) = 0;
	virtual void onMouseUp(int x, int y, MouseButton::Value button) = 0;
	virtual Vec2 getMousePos() const = 0;
	virtual float getMouseRelX() const = 0;
	virtual float getMouseRelY() const = 0;
	virtual void lookAtSelected() = 0;
	virtual bool isOrbitCamera() const = 0;
	virtual void setOrbitCamera(bool enable) = 0;
	virtual const Array<GameObject>& getSelectedEntities() const = 0;
	virtual bool isGameObjectSelected(GameObject gameobject) const = 0;
	virtual void makeParent(GameObject parent, GameObject child) = 0;

	virtual DelegateList<void(const Array<GameObject>&)>& gameobjectSelected() = 0;
	virtual DelegateList<void()>& projectCreated() = 0;
	virtual DelegateList<void()>& projectDestroyed() = 0;

	virtual void addPlugin(Plugin& plugin) = 0;
	virtual void removePlugin(Plugin& plugin) = 0;
	virtual PrefabSystem& getPrefabSystem() = 0;
	virtual Vec3 getCameraRaycastHit() = 0;
	virtual bool isMeasureToolActive() const = 0;
	virtual float getMeasuredDistance() const = 0;
	virtual void toggleMeasure() = 0;
	virtual void setTopView() = 0;
	virtual void setFrontView() = 0;
	virtual void setSideView() = 0;
	virtual class MeasureTool* getMeasureTool() const = 0;
	virtual void makeRelative(char* relative, int max_size, const char* absolute) const = 0;
	virtual void makeAbsolute(char* absolute, int max_size, const char* relative) const = 0;

	virtual void saveUndoStack(const Path& path) = 0;
	virtual bool executeUndoStack(const Path& path) = 0;
	virtual bool runTest(const char* dir, const char* name) = 0;
	virtual void registerEditorCommandCreator(const char* command_type, EditorCommandCreator) = 0;
	virtual bool isGameMode() const = 0;
	virtual void setMouseSensitivity(float x, float y) = 0;
	virtual Vec2 getMouseSensitivity() = 0;
	virtual bool isProjectChanged() const = 0;

protected:
	virtual ~WorldEditor() {}
};
}
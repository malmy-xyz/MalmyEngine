

-- ılk calısan fonk
function init()
	Scene Engine.getScene(Project project, string scene_name)
	Resource Engine.loadResource(Engine engine, string path, string resource_type)
	void Engine.unloadResource(Engine engine, Resource resource)
	GameObject Engine.createGameObject(Project project)
	void Engine.destroyGameObject(Project project, GameObject gameobject)
	GameObject Engine.findByName(Project project, GameObject parent, string name)
	void Engine.loadProject(Engine engine, Project project, string path, function callback)
	void Engine.logError(string text)
	void Engine.logInfo(string text)
	Vec3 Engine.multVecQuat(Vec3 vec, Vec3 axis, number angle)
	Vec3 Engine.multVecQuat(Vec3 vec, Quat quat)
	void Engine.setGameObjectPosition(Project univ, GameObject gameobject, Vec3 position)
	Vec3 Engine.getGameObjectPosition(Project univ, GameObject gameobject)
	void Engine.setGameObjectRotation(Project univ, GameObject gameobject, Vec3 axis, number angle)
	void Engine.setGameObjectRotation(Project univ, GameObject gameobject, Quat rot)
	Quat Engine.getGameObjectRotation(Project univ, GameObject gameobject)
	GameObject Engine.getParent(Project univ, GameObject child)
	void Engine.setParent(Project univ, GameObject parent, GameObject child)
	void Engine.setGameObjectLocalRotation(Project univ, GameObject gameobject, Vec3 axis, number angle)
	void Engine.setGameObjectLocalPosition(Project univ, GameObject gameobject, Vec3 pos)
	Component Engine.createComponent(Scene scene, GameObject gameobject, string type)
	void Engine.destroyComponent(Scene scene, string type, Component component)
	Component Engine.getRenderable(Scene scene, GameObject gameobject)
	ComponentType Engine.getComponentType(string type_name)
	Component Engine.getComponent(Project project, GameObject gameobject, ComponentType)
	Table Engine.instantiatePrefab(Engine engine, Project project, Vec3 position, PrefabResource prefab)
	GameObject Engine.createGameObjectEx(Engine engine, Project project, Table gameobject_description)
	void Engine.nextFrame(Engine engine)
	void Engine.pause(Engine engine, bool pause)
	void Engine.setTimeMultiplier(Engine engine, number multiplier)

end

--her framde calısan fonk
function update(delta_time)
    --Engine.logInfo("update Info " .. tostring(delta_time))
    --Engine.logError("update Error " .. tostring(delta_time))
end

-- ınput algılama
function onInputEvent(event)
    --if event.type == Engine.INPUT_EVENT_BUTTON then
		-- 		
	--elseif event.type == Engine.INPUT_EVENT_AXIS then
		-- 
    --end
end


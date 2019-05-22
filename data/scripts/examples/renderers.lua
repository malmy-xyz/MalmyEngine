--renderer

-- ılk calısan fonk
function init()
    Pipeline Renderer.createPipeline(Engine engine, string pipeline_path)
	void Renderer.destroyPipeline(Pipeline pipeline)
	void Renderer.setPipelineScene(Pipeline pipeline, RenderScene scene)
	void Renderer.pipelineRender(Pipeline pipeline, number width, number height)
	Image Renderer.getRenderBuffer(Pipeline pipeline, string framebuffer_name, number renderbuffer_index)
	string Renderer.getCameraSlot(Camera component)
	Camera Renderer.getCameraComponent(GameObject gameobject)
	Renderable Renderer.getRenderableComponent(GameObject gameobject)
	void Renderer.addDebugLine(Vec3 from, Vec3 to, number rgba_color, number life)
	void Renderer.addDebugCross(Vec3 center, number size, number rgba_color, number life)
	void Renderer.addDebugSphere(Vec3 center, number size, number rgba_color, number life)
	Material Renderer.getTerrainMaterial(Terrain component)
	Texture Renderer.getMaterialTexture(Material material, number texture_index)
	void Renderer.setRenderableMaterial(Scene scene, Renderable component, number material_index, string path)
	void Renderer.setRenderablePath(Scene scene, Renderable component, string path)
	GlobalLight Renderer.getActiveGlobalLight()
	GameObject Renderer.getGlobalLightGameObject(GlobalLight light)
	void Renderer.emitParticle(Scene scene, ParticleEmitter emitter)

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


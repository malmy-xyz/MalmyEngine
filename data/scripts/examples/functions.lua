--functions
--variables
Project g_project
Engine g_engine

g_scene_renderer
g_scene_physics
g_scene_lua_script

-- ılk calısan fonk
function init()
    --Engine.logInfo("init Info")
    --Engine.logError("init Error")
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

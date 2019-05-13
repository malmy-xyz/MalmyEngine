

-- ılk calısan fonk
function init()
    number LuaScript.addScript(Scene scene, Component script)
	void LuaScript.setScriptSource(Scene scene, Component script, number index, string path)
	int LuaScript.getScriptCount(Scene scene, Component script)
	Table LuaScript.getEnvironment(Scene scene, Entity entity, number script_index)
	Timer LuaScript.setTimer(Scene scene, number time_seconds, function callback)
	void LuaScript.cancelTimer(Scene scene, Timer timer)

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


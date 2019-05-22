--apps

-- ılk calısan fonk
function init()
    void App.setProject(Project project)
	void App.loadProject(string path)
	void App.frame()
	void App.exit(int exit_code)
	bool App.isFinished()

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


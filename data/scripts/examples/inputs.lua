

-- ılk calısan fonk
function init()
    Engine.INPUT_DEVICE_KEYBOARD
	Engine.INPUT_DEVICE_MOUSE
	Engine.INPUT_EVENT_BUTTON
	Engine.INPUT_EVENT_AXIS

end

--her framde calısan fonk
function update(delta_time)
    local SPEED = 0.03
	local x = Engine.getInputActionValue(g_engine, X_ACTION) * SPEED
	local y = Engine.getInputActionValue(g_engine, Y_ACTION) * SPEED
	
	if Engine.getInputActionValue(g_engine, SELECT_ACTION) > 0 then
		-- clıcked
	end
end

-- ınput algılama
function onInputEvent(event)
    if event.type == Engine.INPUT_EVENT_BUTTON then
		if event.device.type == Engine.INPUT_DEVICE_KEYBOARD then
			if event.scancode == Engine.INPUT_SCANCODE_W then
				Engine.logInfo("keyboard button event, scancode W")
			end
		end		
	elseif event.type == Engine.INPUT_EVENT_AXIS then
		if event.device.type == Engine.INPUT_DEVICE_MOUSE then
			--yaw = yaw + event.x * -0.005;
            --pitch = pitch + event.y * -0.005;
            
		end
    end
end


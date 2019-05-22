--functions

PLAYER_SPEED = 1
input_enabled = true
temp = 0
ball = nil

--player = -1
--Editor.setPropertyType("player", Editor.GAMEOBJECT_PROPERTY)

-- ilk calisan fonk
function init()
    temp = 0
	--Engine.createComponent(g_project, this, "physical_controller")
end

--her framde calisan fonk
function update(delta_time)
    if not input_enabled then 
        return 
    end
    Engine.setGameObjectPosition(g_project, this, 
    {temp * PLAYER_SPEED, 0, 0})

    if  ball then   
        local x = math.random(-10, 10)
        Physics.applyForceToActor(g_scene_physics, 
        ball, {x * 100, 3000, 0})
        ball = nil
    end
end

-- input algılama
function onInputEvent(event)
	if event.type == Engine.INPUT_EVENT_BUTTON then
		if event.device.type == Engine.INPUT_DEVICE_KEYBOARD then
			if event.scancode == Engine.INPUT_SCANCODE_A then 
                if temp > -6.0 then 
                    temp = temp - 1
                end
			elseif event.scancode == Engine.INPUT_SCANCODE_D then
                if temp < 6.0 then 
                    temp = temp + 1
                end
			end
        end
	end
end

function onCollision(gameobject)

    --Engine.logError("not implemented - onCollision(" .. gameobject .. ")")

    ball = gameobject


end
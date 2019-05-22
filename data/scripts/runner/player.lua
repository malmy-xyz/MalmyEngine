--functions

PLAYER_SPEED = 5
input_enabled = true
temp = 0
ball = nil

zPos = 0

--player = -1
--Editor.setPropertyType("player", Editor.GAMEOBJECT_PROPERTY)

-- ilk calisan fonk
function init()
    temp = 0
	--Engine.createComponent(g_project, this, "physical_controller")
end

--her framde calısan fonk
function update(delta_time)
    if not input_enabled then 
        return 
    end

    --local pos = Engine.getGameObjectPosition(g_project, this) 
    --Engine.logInfo("pos " .. tostring(pos))

    -- -6 ile 6 arasinda hareket edicek
    --temp = temp % 6

    zPos = zPos + delta_time * PLAYER_SPEED

    Engine.setGameObjectPosition(g_project, this, {temp, 0, -zPos})

    if  ball then   
        local x = math.random(-10, 10)
        Physics.applyForceToActor(g_scene_physics, ball, {x * 100, 3000, 0})
        ball = nil
    end


end

-- input algılama
function onInputEvent(event)
	if event.type == Engine.INPUT_EVENT_BUTTON then
		if event.device.type == Engine.INPUT_DEVICE_KEYBOARD then
			if event.scancode == Engine.INPUT_SCANCODE_A then 
                if temp > -2.5 then 
                    temp = temp - 2.5
                end
			elseif event.scancode == Engine.INPUT_SCANCODE_D then
                if temp < 2.5 then 
                    temp = temp + 2.5
                end
			end
        end
    else
        --temp = 0
	end
end

function onCollision(gameobject)

    --Engine.logError("not implemented - onCollision(" .. gameobject .. ")")

    ball = gameobject


end


function onCollision(gameobject)

    --local pos = {}
    --pos = Engine.getGameObjectPosition(g_project, this) 

    --Engine.logInfo("pos " .. tostring(pos))

    	-- get player's position
	
    Engine.setGameObjectPosition(g_project, this, {math.random(50,100), math.random(50,100), math.random(50,100)})

end

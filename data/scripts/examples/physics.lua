--physic

-- ılk calısan fonk
function init()
    bool Physics.isControllerCollisionDown(Scene scene, Component cmp)
	void Physics.moveController(Scene scene, Component cmp, Vec3 velocity, number time_delta)
	number Physics.getActorSpeed(Scene scene, Component cmp)
	Component Physics.getActorComponent(Scene scene, Entity entity)
	void Physics.putToSleep(Scene scene, Component component)
    void Physics.applyForceToActor(Scene scene, Component component, Vec3 force)
    is_hit, hit_entity, hit_position, hit_normal Physics.raycast(Scene scene, Vec3 origin, Vec3 dir, [number layer])


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

function onCollision(gameobject)
    Engine.logError("not implemented - onCollision(" .. gameobject .. ")")
End

function onTrigger(other_gameobject, touch_lost)
    Engine.logError("not implemented - onTrigger(" .. other_gameobject .. ")")
end

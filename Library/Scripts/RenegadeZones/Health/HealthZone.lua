if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Damage / Healing",
        category = "Gameplay Zones",
        role = "SCRIPT",
        description = "Uses the Damage / Healing settings on the owning gameplay zone.",
        properties = {}
    })
end

local function on_event(self, event)
    if not renegade.entity.equals(event.target, self.entity) then return end
    if event.name == "zone.health.enter" then
        renegade.events.send(self.entity, "zone.apply.health.enter", event.payload)
    elseif event.name == "zone.health.exit" then
        renegade.events.send(self.entity, "zone.apply.health.exit", event.payload)
    end
end

return { on_event = on_event }

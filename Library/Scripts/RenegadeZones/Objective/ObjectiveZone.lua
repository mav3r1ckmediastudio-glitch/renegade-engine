if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Objective",
        category = "Gameplay Zones",
        role = "SCRIPT",
        description = "Uses the Objective settings on the owning gameplay zone.",
        properties = {}
    })
end

local function on_event(self, event)
    if not renegade.entity.equals(event.target, self.entity) then return end
    if event.name == "zone.objective.enter" then
        renegade.events.send(self.entity, "zone.apply.objective.enter", event.payload)
    end
end

return { on_event = on_event }

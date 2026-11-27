if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Dialogue",
        category = "Gameplay Zones",
        role = "SCRIPT",
        description = "Uses the Dialogue settings on the owning gameplay zone.",
        properties = {}
    })
end

local function on_event(self, event)
    if not renegade.entity.equals(event.target, self.entity) then return end
    if event.name == "zone.dialogue.enter" then
        renegade.events.send(self.entity, "zone.apply.dialogue.enter", event.payload)
    end
end

return { on_event = on_event }

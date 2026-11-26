if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Win Zone",
        category = "Gameplay Zones",
        role = "SCRIPT",
        description = "Handles player entry and completes the level.",
        properties = {}
    })
end

local function on_event(self, event)
    if event.name == "zone.win.enter" and renegade.entity.equals(event.target, self.entity) then
        renegade.events.send(self.entity, "zone.complete", "")
    end
end

return { on_event = on_event }

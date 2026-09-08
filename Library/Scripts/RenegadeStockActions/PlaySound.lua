if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Play Sound",
        description = "Starts or stops an authored Renegade Sound Source in response to events.",
        category = "Audio",
        role = "ACTION",
        properties = {
            {
                name = "source",
                label = "Sound Source",
                description = "Scene entity containing the authored Sound Source.",
                type = "entity",
            },
            {
                name = "play_event",
                label = "Play Event",
                type = "string",
                default = "play",
            },
            {
                name = "stop_event",
                label = "Stop Event",
                type = "string",
                default = "stop",
            },
            {
                name = "play_on_start",
                label = "Play On Start",
                type = "boolean",
                default = false,
            },
        },
    })
end

local function valid_source(self)
    local source = self.properties.source
    return source and renegade.entity.is_valid(source)
end

return {
    on_start = function(self)
        if self.properties.play_on_start and valid_source(self) then
            renegade.audio.play(self.properties.source)
        end
    end,

    on_event = function(self, event)
        if not valid_source(self) then
            return
        end
        if event.name == self.properties.play_event then
            renegade.audio.play(self.properties.source)
        elseif event.name == self.properties.stop_event then
            renegade.audio.stop(self.properties.source)
        end
    end,
}

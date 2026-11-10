-- Save an effect in the Particle Picker first. Use its exact name or stable ID.
-- Attach this script to the tool/NPC. Use a named child transform or "".
local preset = "YOUR SAVED EFFECT NAME"
local socket_name = ""
local particles = renegade.particles
local effect
return {
    on_start = function(self)
        local err
        effect, err = particles.spawn(preset)
        assert(effect, err)
        local ok
        ok, err = particles.attach(effect, self.entity, socket_name)
        assert(ok, err)
        assert(particles.set_transform(effect, {x=0, y=0, z=0}))
    end,
    on_stop = function(self)
        if effect and renegade.entity.is_valid(effect) then
            assert(particles.destroy(effect))
        end
    end
}

-- Save an effect first; change this to its exact project preset name or ID.
local preset = "YOUR SAVED EFFECT NAME"
local p = renegade.particles
local effect
return {
    on_start = function(self)
        local err
        effect, err = p.spawn(preset, {x=0, y=0, z=0})
        assert(effect, err)
        assert(p.set_lifetime(effect, 1, {
            enabled=true,
            keys={
                {time=0, size=.2, opacity=0},
                {time=.2, size=1, opacity=1},
                {time=1, size=0, opacity=0}
            }
        }))
    end,
    on_stop = function(self)
        if effect and renegade.entity.is_valid(effect) then
            assert(p.destroy(effect))
        end
    end
}

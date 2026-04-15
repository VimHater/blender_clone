-- Pulse: object scales up and down rhythmically
function animate(t, dt)
    local s = 1.0 + 0.5 * math.sin(t * 4)
    set_scale(self.name, s, s, s)
end

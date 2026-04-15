-- Bounce: object moves up and down with a sine wave
function animate(t, dt)
    local x, y, z = get_position(self.name)
    local baseY = 1.0
    set_position(self.name, x, baseY + math.abs(math.sin(t * 3)) * 3, z)
end

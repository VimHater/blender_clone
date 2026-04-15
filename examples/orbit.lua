-- Orbit: object circles around the origin on the XZ plane
function animate(t, dt)
    local radius = 4.0
    local speed = 1.5
    local x = math.cos(t * speed) * radius
    local z = math.sin(t * speed) * radius
    local _, y, _ = get_position(self.name)
    set_position(self.name, x, y, z)
end

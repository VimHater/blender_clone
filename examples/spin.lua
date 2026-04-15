-- Spin: rotates the object around Y axis
function animate(t, dt)
    local rx, ry, rz = get_rotation(self.name)
    set_rotation(self.name, rx, ry + 90 * dt, rz)
end

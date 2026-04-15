-- Color Cycle: smoothly shifts object color through the rainbow
function animate(t, dt)
    local speed = 0.8
    local r = math.floor((math.sin(t * speed) * 0.5 + 0.5) * 255)
    local g = math.floor((math.sin(t * speed + 2.094) * 0.5 + 0.5) * 255)
    local b = math.floor((math.sin(t * speed + 4.189) * 0.5 + 0.5) * 255)
    set_color(self.name, r, g, b, 255)
end

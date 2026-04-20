-- Physics demo: ball drops onto a floor and bounces

-- setup physics on first frame
local initialized = false

function animate(t, dt)
    if not initialized then
        -- floor and wall is static
        set_physics("Floor", true)
        set_static("Floor", true)
        set_restitution("Floor", 0.8)


        set_physics("Wall", true)
        set_static("Wall", true)
        set_restitution("Wall", 0.8)

        set_physics("Wall.1", true)
        set_static("Wall.1", true)
        set_restitution("Wall.1", 0.8)
        -- ball falls with gravity
        set_physics("Ball", true)
        set_gravity("Ball", true)
        set_restitution("Ball", 0.8)
        set_mass("Ball", 1.0)

        -- box falls too
        set_physics("Box", true)
        set_gravity("Box", true)
        set_restitution("Box", 0.5)
        set_mass("Box", 2.0)
        set_velocity("Box", 3, 0, 0)

        initialized = true
    end
end

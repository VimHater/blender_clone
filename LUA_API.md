# Lua Scripting API

## Scripting

Each object can have multiple Lua animation scripts assigned via the Properties panel. Scripts define an `animate(t, dt)` function that runs every frame during play mode.

## Script Structure

```lua
-- Per-object script: 'self' is set as a global before each call
-- self.name  — object name (string)
-- self.id    — object ID (integer)

function animate(t, dt)
    -- t  = elapsed time since play started (seconds)
    -- dt = delta time(seconds)
end
```

## Duration

Scripts can optionally define a `duration` variable (in seconds) at the top level. The engine reads it when loading scripts and sets the timeline length to the longest duration across all scripts. If no script defines `duration`, playback is unlimited.

```lua
duration = 5.0  -- animation lasts 5 seconds

function animate(t, dt)
    local progress = t / duration
    set_position(self.name, 0, progress * 10, 0)
end
```

When a duration is set, the timeline loops at the end. Without it, the timeline runs indefinitely until the user hits Stop.

## Globals

| Name   | Type    | Description                          |
|--------|---------|--------------------------------------|
| `self` | table   | `{name = "...", id = N}` — the object this script is attached to |
| `time` | number  | Elapsed time since play started (seconds) |
| `dt`   | number  | Delta time for this frame (seconds)  |

## API Functions

All functions accept either an object **name** (string) or **id** (integer) as the first argument.

### Transform

#### `get_position(name_or_id) -> x, y, z`
Returns the object's world position as three numbers.

```lua
local x, y, z = get_position(self.name)
```

#### `set_position(name_or_id, x, y, z)`
Sets the object's world position.

```lua
set_position(self.name, 0, 5, 0)
```

#### `get_rotation(name_or_id) -> rx, ry, rz`
Returns the object's rotation in degrees (Euler angles).

```lua
local rx, ry, rz = get_rotation(self.name)
```

#### `set_rotation(name_or_id, rx, ry, rz)`
Sets the object's rotation in degrees.

```lua
set_rotation(self.name, 0, 90, 0)
```

#### `get_scale(name_or_id) -> sx, sy, sz`
Returns the object's scale.

```lua
local sx, sy, sz = get_scale(self.name)
```

#### `set_scale(name_or_id, sx, sy, sz)`
Sets the object's scale.

```lua
set_scale(self.name, 2, 2, 2)
```

### Appearance

#### `get_color(name_or_id) -> r, g, b`
Returns the object's color (0-255 per channel).

```lua
local r, g, b = get_color(self.name)
```

#### `set_color(name_or_id, r, g, b)`
Sets the object's color (0-255 per channel).

```lua
set_color(self.name, 255, 0, 0)  -- red
```

#### `set_visible(name_or_id, visible)`
Shows or hides the object.

```lua
set_visible(self.name, false)
```

### Scene Query

#### `list_objects() -> table`
Returns a table of all active object names.

```lua
local names = list_objects()
for i, name in ipairs(names) do
    print(name)
end
```

#### `get_object_count() -> integer`
Returns the number of active objects in the scene.

```lua
local count = get_object_count()
```

### Output

#### `print(...)`
Prints to the console panel. Accepts multiple arguments (separated by tabs).

```lua
print("position:", x, y, z)
```

## Examples

### Spin

```lua
function animate(t, dt)
    local rx, ry, rz = get_rotation(self.name)
    set_rotation(self.name, rx, ry + 90 * dt, rz)
end
```

### Bounce

```lua
function animate(t, dt)
    local x, y, z = get_position(self.name)
    set_position(self.name, x, 1 + math.abs(math.sin(t * 3)) * 3, z)
end
```

### Orbit

```lua
function animate(t, dt)
    local radius = 4.0
    local speed = 1.5
    local x = math.cos(t * speed) * radius
    local z = math.sin(t * speed) * radius
    local _, y, _ = get_position(self.name)
    set_position(self.name, x, y, z)
end
```

### Pulse

```lua
function animate(t, dt)
    local s = 1.0 + 0.5 * math.sin(t * 4)
    set_scale(self.name, s, s, s)
end
```

### Color Cycle

```lua
function animate(t, dt)
    local speed = 0.8
    local r = math.floor((math.sin(t * speed) * 0.5 + 0.5) * 255)
    local g = math.floor((math.sin(t * speed + 2.094) * 0.5 + 0.5) * 255)
    local b = math.floor((math.sin(t * speed + 4.189) * 0.5 + 0.5) * 255)
    set_color(self.name, r, g, b, 255)
end
```

### Cross-object interaction

```lua
function animate(t, dt)
    -- Move this object to follow "Cube"
    local tx, ty, tz = get_position("Cube")
    set_position(self.name, tx + 2, ty, tz)
end
```

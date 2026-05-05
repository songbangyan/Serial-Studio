# Frame Parser API — Lua

Lua 5.4 mirror of the JavaScript frame-parser API. Functionally identical to
the JS version; choose Lua when your team is already Lua-fluent or when
you want Lua's table semantics for batched parsing.

## Contract

```lua
function parse(frame, separator)
  -- returns array-like table of numbers / strings
end
```

- `frame` is a Lua string (one logical frame).
- `separator` is the source's configured CSV separator string.
- Return a sequence (1-indexed table) whose entries map positionally to
  datasets.
- Return `{}` to skip a frame silently.

## Compatibility shim (LuaCompat)

Serial Studio injects a small compatibility layer so scripts written
against Lua 5.1/5.2 conventions still work:

- `math.log10(x)` — alias for `math.log(x, 10)`.
- `math.pow(a, b)` — alias for `a ^ b`.
- `bit32` — full library (band, bor, bxor, bnot, lshift, rshift, arshift,
  rrotate, lrotate, extract, replace).
- `unpack(t)` — alias for `table.unpack(t)`.

## Sandbox

- No `io.*`, no `os.execute`, no `package.loadlib`, no filesystem.
- `require` is disabled. Single-file scripts.
- Top-level `local` declarations stay scoped to your script (each source
  has its own Lua state).

## Performance

Native Lua is fast. Avoid building full string tables when an in-place
loop will do; avoid `string.format` in the hot path; precompute regex
patterns at script-top-level (they capture as upvalues).

## Examples

### CSV split

```lua
function parse(frame, separator)
  local out = {}
  for value in string.gmatch(frame, "([^" .. separator .. "]+)") do
    out[#out + 1] = tonumber(value) or value
  end
  return out
end
```

### Pattern extraction

```lua
local pattern = "^%$(%a+),(-?%d+%.%d+),(-?%d+%.%d+),(-?%d+)"

function parse(frame)
  local _, lat, lon, sats = string.match(frame, pattern)
  if not lat then
    return {}
  end
  return { tonumber(lat), tonumber(lon), tonumber(sats) }
end
```

### Bit-extraction with bit32

```lua
function parse(frame)
  local b = string.byte(frame, 1)
  return {
    bit32.band(b, 0x0F),
    bit32.rshift(b, 4),
  }
end
```

### Binary frame (escape with `string.byte`)

```lua
function parse(frame)
  if #frame < 8 then return {} end
  local x = string.byte(frame, 1) * 256 + string.byte(frame, 2)
  local y = string.byte(frame, 3) * 256 + string.byte(frame, 4)
  return { x, y }
end
```

## Errors

A Lua error logs a watchdog warning and skips the frame. The Lua state is
preserved across frames; module-level state survives until the source is
reconfigured.

## Choosing JS vs Lua

The two engines have parity for parser scripts. JS gets you a familiar
syntax and good regex; Lua gets you cheaper allocations on hot paths and
slightly faster startup. Pick one per source — you can mix engines across
sources in the same project.

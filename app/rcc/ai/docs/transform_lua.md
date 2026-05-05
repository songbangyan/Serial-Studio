# Per-Dataset Value Transform — Lua

Lua 5.4 mirror of the JS transform API. Use Lua when your team is
Lua-fluent or when you want closure-captured local state semantics.

## Contract

```lua
function transform(value)
  return value
end
```

- `value` is a Lua number (or string for string datasets).
- Return a finite number. NaN/Inf or non-numeric returns fall back to the
  raw value silently.

## Isolation

The runtime invokes `luaL_dostring(your_code)` once per dataset. Top-level
`local` declarations become **upvalues** captured by the `transform`
closure — so two datasets that define `local ema = 0` get independent
state, even though the Lua state itself is shared across the source.

```lua
local ema = 0
local alpha = 0.2

function transform(value)
  ema = alpha * value + (1 - alpha) * ema
  return ema
end
```

## Data table API

Functions injected into the Lua state:

```lua
tableGet(tableName, registerName)      -- -> number | string
tableSet(tableName, registerName, v)    -- user tables only
datasetGetRaw(uniqueId)                 -- raw value, this frame
datasetGetFinal(uniqueId)               -- final value of an earlier dataset
```

## Compatibility shim (LuaCompat)

Same shim as the parser API:

- `math.log10(x)` and `math.pow(a, b)` aliases.
- Full `bit32` library.
- `unpack(t)` for `table.unpack(t)`.

## Examples

### EMA smoothing

```lua
local ema = 0
local alpha = 0.2

function transform(value)
  ema = alpha * value + (1 - alpha) * ema
  return ema
end
```

### Calibration

```lua
function transform(value)
  local offset = tableGet("Calibration", "offset")
  local scale  = tableGet("Calibration", "scale")
  return (value - offset) * scale
end
```

### Cross-dataset compute

```lua
function transform(dx)
  local dt = datasetGetRaw("dt_ms")
  if dt and dt > 0 then
    return (dx / dt) * 1000
  end
  return 0
end
```

### Hysteresis

```lua
local last = 0
local threshold = 0.05

function transform(value)
  if math.abs(value - last) >= threshold then
    last = value
  end
  return last
end
```

## Performance

Lua is fast at the call boundary. Avoid `string.format` in the hot path,
avoid `pcall` unless you actually expect failures, and prefer arithmetic
to table lookups when you can.

## Errors

A Lua error logs a watchdog warning and the raw value is used. Returning a
non-number falls back silently. Don't rely on errors for control flow.

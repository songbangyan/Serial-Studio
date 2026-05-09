# Per-Dataset Value Transforms

Transforms run on every parsed frame, after the frame parser, before the
dashboard sees the value. They turn a raw value into the displayed value.

## Pick Lua first

Both Lua and JavaScript work, but **Lua is the recommended default** —
it's measurably faster on the hotpath at typical telemetry rates and
the embedded interpreter has a lighter per-call cost than `QJSEngine`.
Use JavaScript only when you need a JS-specific feature (regex
flavours, `JSON.stringify`, etc.).

When you push transform code, ALWAYS pass `language` so the dataset's
`transformLanguage` is locked to the syntax you wrote. A mismatch is a
silent compile failure — the dashboard will show the raw value with no
visible error. Two ways to set both at once:

```
project.dataset.setTransformCode {groupId, datasetId, code, language: 1}
project.dataset.update            {groupId, datasetId, transformCode, transformLanguage: 1}
```

## Adding a compute-only dataset (no parser slot)

A dataset whose value is *computed* by a transform — derived from peers,
table registers, or constants — is **virtual**. Set `virtual: true` on
creation, otherwise the runtime tries to read `channels[index - 1]`
from the parser output and the dataset ends up empty.

```
project.dataset.add {groupId, options: ["plot"]}  // creates dataset N
project.dataset.update {
  groupId, datasetId: N,
  virtual: true,                                  // compute-only
  title: "Speed",
  units: "m/s",
  transformLanguage: 1,                           // 1 = Lua
  transformCode: "..."
}
```

**Auto-detect**: when the project saves, the save path inspects each
dataset's transform body. If `transformCode` is non-empty AND the body
**never references `value`**, the dataset is auto-flagged `virtual:
true`. So a transform like `function transform(_v) return
datasetGetFinal(uid_speed) * 0.27778 end` is detected as virtual on
save even if you forgot to set the flag. Manual setting is still
recommended for clarity, because the auto-detect only fires at save
time -- if you push the transform and run live before the next save,
the dataset still misses its value.

If `virtual=false` and `index<=0` after a `setTransformCode`, the API
returns a `hint` field telling you to flip `virtual`. Listen to it.

### Why prefer virtual datasets over an extra parser slot

You COULD parse-and-emit a derived value from the frame parser
directly (so the parser writes `channels[N] = computeSpeed(...)` and
a regular dataset reads slot N). Don't. Virtual datasets are the
right shape because:

- **Separation of concerns.** Frame parsers turn bytes into raw
  channels — that's their whole job. Computation, calibration, and
  derivation belong in transforms. Mixing both in the parser turns
  it into an opaque blob that's painful to debug with `dryRun`.
- **Testability.** `project.dataset.transform.dryRun{values, code}`
  exercises a transform in isolation. You can't dry-run a derived
  channel that lives inside `parse()`; you'd have to invent fake
  byte frames every iteration.
- **Cross-source reach.** A virtual dataset's transform can read
  `datasetGetFinal(uniqueId)` of any dataset, including peers from
  *other* sources via the shared data table. A parser only sees its
  own source's bytes.
- **Unit + range hygiene.** Virtual datasets carry their own
  `units`, `plotMin/Max`, `widgetMin/Max`, alarms, and widget
  bitflags. They show up cleanly in the project tree, the dashboard,
  and CSV/MDF4 exports. A computation hidden inside the parser has
  no presence in the schema.
- **Templates.** A virtual dataset (transform + units + widget
  config) is the unit you copy across projects. Parser code is
  source-bus-specific; transforms are portable.
- **Performance.** Virtual datasets share the source's transform
  engine — no extra QJSEngine / Lua state per derivation. A bloated
  parser, by contrast, runs every byte through one big function on
  every frame, allocating intermediate arrays.

Use a parser-emitted slot only when the derivation needs raw bytes
that aren't otherwise exposed (uncommon — most cases are downstream
arithmetic on already-extracted channels).

## Contract

```lua
function transform(value)
  return value  -- must return a finite number or string
end
```

```js
function transform(value) {
  return value;  // must return a finite number (or QString-coercible)
}
```

For numeric datasets, `value` is a `Number`. For string datasets, it's a
`String`. Returning `NaN`, `Infinity`, or `undefined` rejects the output
and the dashboard shows the raw value instead. No exception, just a
silent fallback.

## Per-dataset isolation

Your transform script is wrapped in an IIFE at compile time:

```js
(function() {
  /* your code */
  return typeof transform === 'function' ? transform : null;
})()
```

Top-level `var/let/const` are private per dataset, even when several
datasets in the same source share a JS engine. Two datasets can both
declare `let alpha = 0.2` without clobbering each other.

## Iteration workflow

1. Read the dataset's current transform:
   `project.dataset.list` returns `transformCode` per dataset; or fetch
   `project.frameParser.getCode` for context on what `value` looks like.
2. Dry-run: `project.dataset.transform.dryRun{code, language, values}`
   compiles + runs `transform()` against an array of sample inputs.
   Returns the per-input outputs. Iterate on the code until outputs look
   right.
3. Push: `project.dataset.setTransformCode{groupId, datasetId, code}`.

## Tables: the central data bus

Transforms read and write two kinds of registers:

**System table** (`__datasets__`, always present): two registers per
dataset — `raw:<uniqueId>` and `final:<uniqueId>`. Read-only. Convenience
helpers: `datasetGetRaw(uniqueId)` and `datasetGetFinal(uniqueId)`. Use
those instead of `tableGet('__datasets__', 'raw:<uid>')`.

**User tables**: project-defined. Two register types:

- `Constant`: single value across the session. Set when declared. Use for
  calibration coefficients, lookup tables, configuration flags.
- `Computed`: resets to `defaultValue` at the start of every frame.
  Writable from transforms. Use for per-frame accumulators, derived
  values that another transform reads later in the same frame.

```js
tableGet(tableName, registerName)              // -> number | string
tableSet(tableName, registerName, value)       // user tables only
datasetGetRaw(uniqueId)                        // any dataset, this frame
datasetGetFinal(uniqueId)                      // EARLIER datasets only
```

`uniqueId` is a stable INTEGER, not a name. Get it from
`project.dataset.list` (`uniqueId` field) or compute as
`sourceId * 1_000_000 + groupId * 10_000 + datasetId`.

## Processing order

Datasets are processed in group-array then dataset-array order. Inside a
transform you can read:

- raw values of **all** datasets in this frame (parser already ran)
- final values of **earlier** datasets only

Trying to read `datasetGetFinal` of a dataset processed later returns 0.

## When to use what

- **Per-dataset state across frames** (EMA, last value, deadband): use a
  top-level `let` in the transform. Cheaper than a table register.
- **Shared state across datasets** (calibration constants used by N
  channels, lookup tables): use a Constant register in a user table.
- **Cross-dataset compute within one frame** (speed from dx and dt that
  arrive together): use `datasetGetRaw` to read a peer, OR write to a
  Computed register in the earlier transform and `tableGet` it from the
  later one.

## Examples (Lua — preferred)

```lua
-- EMA smoothing (per-dataset state via upvalue)
local ema = 0
local alpha = 0.2
function transform(value)
  ema = alpha * value + (1 - alpha) * ema
  return ema
end

-- Calibration from constants table
function transform(value)
  local offset = tableGet("Calibration", "offset")
  local scale  = tableGet("Calibration", "scale")
  return (value - offset) * scale
end

-- Cross-dataset speed (DT_MS_UID from project.dataset.list)
local DT_MS_UID = 10003
function transform(dx)
  local dt = datasetGetRaw(DT_MS_UID)
  if dt and dt > 0 then return (dx / dt) * 1000 end
  return 0
end
```

## Examples (JavaScript — when Lua won't do)

```js
let ema = 0;
const alpha = 0.2;
function transform(value) {
  ema = alpha * value + (1 - alpha) * ema;
  return ema;
}
```

For ~20 more reference transforms (clamp, dead-zone, ADC-to-voltage,
celsius/fahrenheit, accumulator, autozero, bit extract, ...), call
`scripts.list{kind: "transform_lua"}` or
`scripts.list{kind: "transform_js"}`.

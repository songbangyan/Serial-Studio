# Per-Dataset Value Transform — JavaScript

A transform turns a raw numeric (or string) dataset value into the value the
dashboard plots, displays, and exports. Transforms run on every frame, in
declared dataset order. They are independent per dataset.

## Contract

```js
function transform(value) {
  return value;  // must return a number (or QString-coercible value)
}
```

- `value` is the current frame's raw value for this dataset, after the parser
  ran. For numeric datasets it is a `Number`; for string datasets it is a
  `String`.
- Return a finite number. Returning `NaN`, `Infinity`, or `undefined`
  rejects the transform's output and the dashboard uses the **raw** value
  instead. (No exception, just a fallback.)

## Isolation

Your script is automatically wrapped in an IIFE at compile time:

```js
(function() {
  /* your code */
  return typeof transform === 'function' ? transform : null;
})()
```

That means top-level `var/let/const` are private per dataset, even when
several datasets in the same source share a JS engine. Two datasets can
both define `let alpha = 0.2` without clobbering each other.

## Data table API — the central data bus

Transforms read from and write to two kinds of registers.

**System table** (`__datasets__`, always present): two registers per
dataset, `raw:<uniqueId>` and `final:<uniqueId>`, populated by the
FrameBuilder during parsing. Read-only from a transform. You almost
never read it via `tableGet` — use the convenience helpers below, which
are faster and more legible.

**User tables**: defined in the project (create via
`project.dataTable.add` + `project.dataTable.addRegister`). Each register
is one of two types:

- `Constant` — single immutable value across the session. Set once at
  declaration; every `tableGet` returns it. Use for calibration
  coefficients, lookup tables, configuration flags.
- `Computed` — resets to its `defaultValue` at the **start of every
  parsed frame**. Writable from transforms via `tableSet`. Use for
  per-frame accumulators, derived values, intermediate results another
  transform reads later in the same frame.

Functions injected into your scope:

```js
tableGet(tableName, registerName)              // -> number | string
tableSet(tableName, registerName, value)       // user-table writes only
datasetGetRaw(uniqueId)                        // raw value of any dataset
datasetGetFinal(uniqueId)                      // final value of an EARLIER dataset (this frame)
```

`uniqueId` is an OPAQUE integer that uniquely identifies a dataset
within the project. It comes back from `project.dataset.list` (under
`uniqueId`), from `project.snapshot`, and from the resolvers
`project.dataset.getByPath { path: "Group/Dataset" }` /
`getByTitle` / `getByUniqueId`.

**Treat the value as opaque.** It happens to be derived from
`(sourceId, groupId, datasetId)`, but reordering changes those
numbers. Resolve once via the API and pass the resulting integer into
`datasetGetRaw / datasetGetFinal` -- never recompute it.

## Processing order

Datasets are processed in group-array then dataset-array order. A transform
can read:

- raw values of **all** datasets in this frame (parser already ran)
- final values of **earlier** datasets only

Trying to read `datasetGetFinal` of a dataset processed later returns
nil/empty AND logs a one-shot warning to the runtime console. Use
`project.dataset.getExecutionOrder` to confirm the order of execution
when peer reads return stale or empty values.

## When to use a table vs a transform local

- **Per-dataset state** (EMA across frames, last-seen value, deadband
  hysteresis): use a top-level `let`/`var` in your transform script. The
  IIFE wrapping keeps each dataset's locals private. Cheaper than a
  table register.
- **Shared state across datasets** (calibration constants used by N
  channels, a global counter, lookup tables): use a Constant or Computed
  register in a user table.
- **Cross-dataset derived values within one frame** (compute speed from
  dx and dt that arrive in the same frame): use `datasetGetRaw` to read a
  peer dataset, OR write to a Computed register in the earlier transform
  and `tableGet` it from the later one.

## Examples

### EMA smoothing (per-dataset state)

```js
let ema = 0;
const alpha = 0.2;

function transform(value) {
  ema = alpha * value + (1 - alpha) * ema;
  return ema;
}
```

Two datasets in the same project can both use this template; the IIFE
wrapping keeps each dataset's `ema` independent.

### Calibration from a constants table

```js
function transform(value) {
  const offset = tableGet('Calibration', 'offset');
  const scale = tableGet('Calibration', 'scale');
  return (value - offset) * scale;
}
```

### Cross-dataset compute (speed from dx and dt)

`uniqueId` is a number, not a name. Read it from
`project.dataset.list` and pin it in a top-level constant.

```js
const DT_MS_UID = 10003;       // <- from project.dataset.list

function transform(dx) {
  const dt = datasetGetRaw(DT_MS_UID);
  return dt > 0 ? (dx / dt) * 1000 : 0;
}
```

### Per-frame accumulator via Computed register

```js
// Computed register "FuelTotal" with defaultValue 0; resets each frame.
function transform(litresPerHour) {
  // ms since last frame, populated by an earlier transform on dt_ms
  const dtMs = tableGet('FrameTimer', 'dtMs');
  const delta = (litresPerHour / 3600.0) * (dtMs / 1000.0);
  const total = tableGet('Trip', 'litresUsed') + delta;
  tableSet('Trip', 'litresUsed', total);
  return total;
}
```

## Performance

Transforms run on every frame for every dataset that defines one. Cheap:
arithmetic, single `tableGet` calls, branchless math. Avoid:

- `JSON.parse`, `JSON.stringify`
- Allocating arrays / objects per call
- Try/catch in the hot path

## Errors

Returning `NaN` or `Infinity` falls back to the raw value silently. Throwing
an exception logs a watchdog warning and uses the raw value. Don't rely on
exceptions for control flow.

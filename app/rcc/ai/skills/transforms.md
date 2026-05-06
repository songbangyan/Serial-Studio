# Per-Dataset Value Transforms

Transforms run on every parsed frame, after the frame parser, before the
dashboard sees the value. They turn a raw value into the displayed value.

## Contract

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

## Examples

```js
// EMA smoothing (per-dataset state)
let ema = 0;
const alpha = 0.2;
function transform(value) {
  ema = alpha * value + (1 - alpha) * ema;
  return ema;
}

// Calibration from constants table
function transform(value) {
  const offset = tableGet('Calibration', 'offset');
  const scale  = tableGet('Calibration', 'scale');
  return (value - offset) * scale;
}

// Cross-dataset speed (DT_MS_UID from project.dataset.list)
const DT_MS_UID = 10003;
function transform(dx) {
  const dt = datasetGetRaw(DT_MS_UID);
  return dt > 0 ? (dx / dt) * 1000 : 0;
}
```

For ~20 more reference transforms (clamp, dead-zone, ADC-to-voltage,
celsius/fahrenheit, accumulator, autozero, bit extract, ...), call
`scripts.list{kind: "transform_js"}` or `scripts.list{kind: "transform_lua"}`.

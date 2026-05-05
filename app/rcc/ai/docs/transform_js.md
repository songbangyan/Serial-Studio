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

## Data table API

Transforms can read from and write to two kinds of registers:

- The **system table** (`__datasets__`): two registers per dataset,
  `raw:<uniqueId>` and `final:<uniqueId>`. Populated by the FrameBuilder
  during parsing. Read-only from a transform.
- **User tables**: defined in the project's "tables" section. Each register
  is `Constant` (read-only at runtime) or `Computed` (resets per frame,
  writable from transforms).

Functions injected into your scope:

```js
tableGet(tableName, registerName)              // -> number | string
tableSet(tableName, registerName, value)       // user-table writes only
datasetGetRaw(uniqueId)                        // raw value of any dataset
datasetGetFinal(uniqueId)                      // final value of an EARLIER dataset (this frame)
```

`uniqueId` is the dataset's stable string identifier (set in the project
editor; visible in the Project Editor's dataset detail).

## Processing order

Datasets are processed in group-array then dataset-array order. A transform
can read:

- raw values of **all** datasets in this frame (parser already ran)
- final values of **earlier** datasets only

Trying to read `datasetGetFinal` of a dataset processed later returns `0`
without error.

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

### Cross-dataset compute (dataset 'speed' from dx and dt)

```js
function transform(dx) {
  const dt = datasetGetRaw('dt_ms');
  return dt > 0 ? (dx / dt) * 1000 : 0;
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

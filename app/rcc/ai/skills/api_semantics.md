# API Semantics — the corners that don't fit on a schema

Schemas tell you what's accepted. This skill tells you what each value
actually MEANS, when things execute, and what the corners look like.
Load it whenever a behavior question (timing, lookup, identity) is
about to send you down a debugging hole.

## Identity — datasetId vs index vs uniqueId

A dataset has three integer-shaped identifiers, and they are NOT the
same thing.

| Field        | Set by      | Used for                                         |
|--------------|-------------|--------------------------------------------------|
| `datasetId`  | Auto, on `dataset.add` | CRUD APIs: `project.dataset.update`, `project.dataset.delete`, every `setOption*`. Position within the group. |
| `index`      | User        | Position in the parser's output array (1-based). The parser's `parse(frame)[i]` populates the dataset whose `index == i + 1`. |
| `uniqueId`   | Derived     | Runtime key for `datasetGetRaw / datasetGetFinal` and the `__datasets__` system table. `sourceId * 1_000_000 + groupId * 10_000 + datasetId`. |

**Don't compute `uniqueId` by hand.** `project.dataset.list` returns it
on every dataset; `project.group.list`'s `datasetSummary` returns it
too. Read it, don't math it.

When users say "the third dataset," ask them which they mean — the
project-editor row order is `datasetId`, the parser-output position is
`index`. They CAN diverge if `index` was edited.

## Frame execution cycle — what runs in what order

```
   bytes from driver
         │
         ▼
   FrameReader splits on delimiters, stamps each frame with a timestamp
         │
         ▼
   FrameBuilder: parse(frame) -> array of channel strings (or 2D array)
         │
         │   for each parsed row (1+):
         ▼
   ┌─────────────────────────────────────────────────────────────┐
   │ Computed registers RESET (Constant registers untouched)     │
   ├─────────────────────────────────────────────────────────────┤
   │ for each group (project order):                              │
   │   for each dataset (project order):                          │
   │     1. raw = channels[index - 1]                             │
   │     2. setDatasetRaw(uniqueId, raw)                          │
   │     3. if transformCode: final = transform(raw)              │
   │        - sees: all raw, final of EARLIER datasets only,      │
   │                Constant + this-frame's Computed writes        │
   │     4. setDatasetFinal(uniqueId, final)                      │
   ├─────────────────────────────────────────────────────────────┤
   │ TimestampedFramePtr published once, shared by all consumers  │
   └─────────────────────────────────────────────────────────────┘
         │
         ├─► Dashboard widgets (visualization update on UI tick ~24 Hz)
         │       └─► Painter onFrame() then paint(ctx,w,h) per painter widget
         ├─► CSV / MDF4 export workers (lock-free queue, batch on worker thread)
         ├─► API / gRPC / MQTT publishers
         └─► Session DB writer (Pro)
```

The cycle in prose form, for each parsed frame in a source:

1. **Computed registers reset.** `m_tableStore.resetComputedRegisters()`
   runs once at the top of the frame, BEFORE any dataset is touched.
   Constant registers don't reset (they're project-static).
2. **Datasets walk in (group order, then dataset order within group).**
   For each dataset:
   1. Read raw value from `parse()[index - 1]`.
   2. Write to the data table: `setDatasetRaw(uniqueId, value)`.
   3. If `transformCode` is non-empty, call `transform(value)`. The
      transform sees:
      - All Constant registers (read-only).
      - Reset Computed registers + writes from EARLIER datasets in
        this same frame (via `tableSet`).
      - Raw values of EVERY dataset (already populated above).
      - Final values of EARLIER datasets in this frame only.
   4. Write the result to the data table: `setDatasetFinal(uniqueId, value)`.
3. **TimestampedFramePtr fans out.** One shared object reaches the
   dashboard, CSV/MDF4 export, the API server, gRPC, MQTT, and
   Sessions. They all see the same final values.

So: a transform on dataset C in group 1 can read final values of
datasets A and B that came earlier in the same group (or earlier
groups). It cannot read final values of D or later — they haven't run
yet.

**Painters run on the UI refresh tick, NOT on every parsed frame.** The
dashboard repaints at ~24 Hz; if frames arrive faster, the painter
samples whichever frame was latest at tick time. A painter reading
`datasetGetFinal(uid)` always sees the most recent fully-processed
value, but might skip intermediate frames between two `onFrame()`
calls. Don't put per-frame accumulators in painter `onFrame()` — that
belongs in a transform, where every frame fires.

## Cross-source transforms — what's visible

`hotpathRxSourceFrame(sourceId, data)` processes one source's frame at
a time. Each source has its own dataset list and parser.

The **data table store is shared across sources**. So:

- A transform in `sourceId=0` can `tableGet` / `datasetGetFinal` values
  written by `sourceId=1` — but it sees whatever was *last written*,
  which is `sourceId=1`'s previous frame, not its current one.
- There's no cross-source synchronization. If you need a calculation
  that depends on two sources arriving "together," your project layout
  is wrong — model it as one source with a parser that emits both
  channels, or accept the staleness.

## Frame-parser batching — timestamps per row

When a frame parser returns a 2D array (N rows × C channels), the
FrameBuilder treats each row as its own logical frame and assigns
timestamps as `chunk.timestamp + step * i` where `step` is the
driver-provided cadence in nanoseconds. So:

- Audio at 48 kHz with 256-sample chunks → step ≈ 5333 ns per row.
- Each row's `TimestampedFramePtr` carries the correct interpolated
  time. CSV/MDF4 export and the dashboard see strictly monotonic time.
- Frame metadata (group titles, dataset definitions) is shared across
  rows — only the per-channel values and timestamp differ.

When `step` isn't set by the driver, FrameBuilder estimates it from
the chunk size and the previous chunk's timestamp. Audio drivers
populate it directly; UART / network usually leave it 0.

## Data table edges — missing keys, type coercion

`tableGet(table, register)` and `datasetGetRaw / datasetGetFinal(uid)`:

- **Missing key returns `undefined` (JS) / `nil` (Lua).** No throw, no
  zero. Always `if (val === undefined) ...` or `if val == nil then ...`.
- **Numeric vs string is preserved.** A register written by `tableSet`
  with a number stays numeric; written with a string stays string. Don't
  rely on coercion — when you need a number, `Number(val)` /
  `tonumber(val)` first.
- **`tableSet` only writes Computed registers.** Writing to a Constant
  register name silently no-ops.

`__datasets__` is the auto-generated system table. Each dataset has
two registers: `raw:<uniqueId>` and `final:<uniqueId>`. You almost
never read those directly — `datasetGetRaw` / `datasetGetFinal` are
the typed shortcuts and avoid the string-key arithmetic.

## DatasetOption bitflag, summarized

Numbers used by `project.dataset.setOptions`, `setOption`, and the
`options` field on `dataset.add`:

```
1  = Plot           (JSON: graph)
2  = FFT            (JSON: fft)
4  = Bar            (JSON: widget = "bar")     ┐
8  = Gauge          (JSON: widget = "gauge")   ├ mutually exclusive
16 = Compass        (JSON: widget = "compass") ┘
32 = LED            (JSON: led)
64 = Waterfall (Pro)(JSON: waterfall)
```

These do NOT line up with `DashboardWidget` enum values used by
`project.workspace.addWidget`. See `dashboard_layout` for the full
bitflag → DashboardWidget mapping.

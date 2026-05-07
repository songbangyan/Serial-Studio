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
| `uniqueId`   | Derived     | OPAQUE runtime handle for `datasetGetRaw / datasetGetFinal` and the `__datasets__` system table. |

**Treat `uniqueId` as opaque.** It happens to be computed as
`sourceId*1_000_000 + groupId*10_000 + datasetId`, but **arithmetic on
it is fragile** -- reordering a group or moving a dataset changes
those numbers. The right pattern:

```
// Looking up a dataset by name, in scripts or tools
project.dataset.getByPath { path: "Audio/Channel A" }
project.dataset.getByTitle { title: "Channel A", groupId: 0 }
project.dataset.getByUniqueId { uniqueId: 10001 }

// Inside transforms / painter scripts: read peers via the API
const v = datasetGetFinal(uid)        // uid from the response above
```

`project.dataset.list`, `project.group.list`'s `datasetSummary`, and
`project.snapshot` all return `uniqueId` on every dataset. Read it
fresh after every move/duplicate/delete -- don't cache it across
mutations.

When users say "the third dataset," ask them which they mean — the
project-editor row order is `datasetId`, the parser-output position is
`index`. They CAN diverge if `index` was edited.

Workspace IDs are a separate range: **always >= 1000**. Auto-generated
tabs start at 1000-1001 and per-group at 1002+; user-created
workspaces start at 5000.

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

- **Missing key returns `undefined` (JS) / `nil` (Lua) AND logs a
  one-shot warning per (table, register) miss to the runtime console.**
  No throw. Always `if (val === undefined) ...` or `if val == nil
  then ...`. The warning helps catch typos -- look for
  `[DataTableStore] Missing register ...` in the runtime log on first
  occurrence.
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

## Dataset options — slugs preferred, bitflags accepted

`project.dataset.setOptions`, `setOption`, and the `options` field on
`dataset.add` accept **string slugs** (preferred) or integer bitflags
(back-compat).

| Slug          | Bit | JSON key (`.ssproj`)            |
|---------------|-----|---------------------------------|
| `"plot"`      | 1   | `graph: true`                   |
| `"fft"`       | 2   | `fft: true`                     |
| `"bar"`       | 4   | `widget: "bar"`     ┐           |
| `"gauge"`     | 8   | `widget: "gauge"`   ├ mutually  |
| `"compass"`   | 16  | `widget: "compass"` ┘ exclusive |
| `"led"`       | 32  | `led: true`                     |
| `"waterfall"` | 64  | `waterfall: true`  (Pro)        |

```
project.dataset.setOptions { groupId, datasetId, options: ["plot","fft"] }
project.dataset.setOption  { groupId, datasetId, option: "fft", enabled: true }
```

The integer bitflags above do NOT line up with the `DashboardWidget`
enum integers used by `project.workspace.addWidget` -- which is
exactly why slugs exist. Use slugs and the collision disappears. See
`dashboard_layout` for the full table.

## Error categories

Failed tool calls carry `error.data.category`. Distinct categories
that matter:

- `validation_failed` -- fix args; schema is in `error.data.inputSchema`.
- `unknown_command` -- look at `error.data.did_you_mean`.
- `license_required` -- propose a non-Pro path.
- `connection_lost` -- ask the user to reconnect; don't retry.
- `script_compile_failed` -- iterate via `frameParser.dryCompile` (compile
  only) or `frameParser.dryRun` / `transform.dryRun` (compile + execute).
- `bus_busy` -- brief retry, then surface.
- `permission_denied` -- OS-level (filesystem, network) refusal.
- `hardware_write_blocked` -- the runtime refuses io.* / console.send
  writes for safety. Distinct from `permission_denied`. Explain to the
  user that hardware writes are gated; suggest building an Output
  Control tile so the user triggers the write themselves.
- `file_not_found` -- ask for the right path.
- `execution_error` -- everything else; read the message.

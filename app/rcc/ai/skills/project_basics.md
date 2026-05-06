# Project Basics — sources, groups, datasets, actions

A Serial Studio project is the bundle of configuration the dashboard
needs to interpret data from one or more devices.

## The four nouns

- **Source**: one connected device. Identified by sourceId. Has a bus
  type (UART, Network, BLE, ...), bus-specific config (port, baud,
  host), and its own frame parser. A project can have multiple sources;
  `sourceId = 0` is the default.
- **Group**: a logical bundle of related datasets, with a display widget
  type (DataGrid, MultiPlot, Accelerometer, GPS, Painter, ...). One
  group → one tile-shape on the dashboard.
- **Dataset**: one channel of incoming numeric (or string) data. Maps
  one position in the parser's output array. Has visualization options
  (plot, FFT, bar, gauge, LED, compass, waterfall) as bit flags.
- **Action**: a button on the toolbar that transmits a fixed payload.
  Optional repeat-on-timer.

## The relationships

```
Project
├── sources[]            (one or more connected devices)
│   └── frameParser      (per-source JS or Lua script)
├── groups[]             (visualization bundles)
│   └── datasets[]       (per-channel data)
│       └── transformCode (optional, runs after parse)
├── actions[]            (toolbar buttons)
├── workspaces[]         (dashboard tabs; pin widgets here)
└── tables[]             (data-bus registers; central state)
```

## Listing the project

The model's most-used reads:

```
project.getStatus       // top-level: title, modified, mode, counts
project.group.list      // every group + datasetSummary + compatibleWidgetTypes
project.dataset.list    // every dataset across all groups
project.source.list     // every source with bus + parser info
project.workspace.list  // every dashboard tab
project.dataTable.list  // every user-defined data table
project.validate        // semantic consistency check
meta.snapshot           // composite of above + io/dashboard/etc.
```

`group.list` is denser than `dataset.list` in most cases — it shows
groups and a summary of each dataset (title, units, uniqueId, etc.).

## Identifiers — what's stable, what's not

- `sourceId`, `groupId`, `datasetId`, `actionId`, `widgetId`,
  `workspaceId` — integer ids, ASSIGNED on creation. They're stable for
  the lifetime of the project.
- `uniqueId` (on datasets) — derived as
  `sourceId * 1_000_000 + groupId * 10_000 + datasetId`. The runtime
  uses uniqueId in `datasetGetRaw/datasetGetFinal` and in the system
  `__datasets__` table. Get it from `project.dataset.list` or
  compute. Stable within a project.
- `index` (on datasets) — the position in the parser's output array.
  1-based. The user sets this; the parser's `parse(frame)[i]` maps to
  the dataset whose `index` is `i + 1`.

## Operation modes

The dashboard has three operation modes (`AppState::operationMode`):

- **0 = ProjectFile**: the normal one. Project loaded from a `.ssproj`
  file, full editor + dashboard.
- **1 = ConsoleOnly**: terminal-only. Frame parser bypassed; raw bytes
  go straight to the console. No dashboard. Good for raw-protocol
  debugging.
- **2 = QuickPlot**: line-based input (CR/LF/CRLF), comma-separated,
  auto-generates groups/datasets/widgets. No project file. Good for
  one-shot prototyping.

Mode is sticky (persisted to QSettings). Switch with
`dashboard.setOperationMode{mode}`. `project.open` auto-switches to
ProjectFile.

## The auto-save loop

Every successful mutating tool call schedules a debounced save (~800ms)
to the project's existing file path. So:

- Don't call `project.save{}` after every edit — it's redundant.
- Do call `project.save{filePath: "..."}` when the user wants Save As.
- New/empty projects without a file path skip auto-save (nothing to
  save to). The user must explicitly save with a path.

## Settings that look project-shaped but live elsewhere

A few things you'd intuitively look for under `project.*` actually live
under other scopes because they're dashboard-wide preferences, not
project state:

| Setting                       | Command                          |
|-------------------------------|----------------------------------|
| Plot point count (rolling history per series) | `dashboard.setPoints{points}` |
| Theme                         | `ui.theme.*` / `--theme` flag     |
| Operation mode                | `dashboard.setOperationMode`     |
| Console terminal display      | `console.set*`                   |

If the user asks for "more points on the plot" or "longer plot history",
that's `dashboard.setPoints`, not a `project.*` command.

## Glossary — terms that look interchangeable but aren't

These are the pairs and triples that LLMs (and humans) routinely
conflate. Memorize them once.

### Dataset identifiers

| Term         | Range            | Set by    | Used for                                      |
|--------------|------------------|-----------|-----------------------------------------------|
| `datasetId`  | per-group, 0..N  | API auto  | CRUD: `dataset.update / delete / setOptions`  |
| `index`      | 1-based int      | User      | Position in `parse(frame)` output array       |
| `uniqueId`   | global int       | Derived   | Runtime key for `datasetGetRaw / Final`       |

`uniqueId = sourceId * 1_000_000 + groupId * 10_000 + datasetId`. Read
it from `dataset.list` — don't compute by hand.

### Numeric ranges on a dataset

A single dataset can carry **three independent min/max pairs**, each
driving a different surface:

| Range pair             | Drives                                                                |
|------------------------|-----------------------------------------------------------------------|
| `plotMin` / `plotMax`  | Y-axis on Plot/MultiPlot                                              |
| `widgetMin` / `widgetMax` | Gauge / Bar / Compass scales (radial dial limits, bar fill range)  |
| `fftMin` / `fftMax`    | dB floor/ceiling on FFT and Waterfall plots                           |

Setting one doesn't cascade. A gauge that runs 0–360 (`widgetMin/Max`)
might still want the underlying plot Y-axis at -50–50 (`plotMin/Max`).

### Frame detection enum (`frameDetection` field)

```
0 = EndDelimiterOnly      most common; e.g. line-based `\n`
1 = StartAndEndDelimiter  bracketed frames; e.g. `$...;`
2 = NoDelimiters          fixed-length packets (binary protocols)
3 = StartDelimiterOnly    start marker, length follows
```

### Decoder method enum (`decoderMethod` / `decoder` field)

```
0 = PlainText     UTF-8 text frames
1 = Hexadecimal   "DEADBEEF" hex-encoded bytes
2 = Base64        base64-encoded binary
3 = Binary        raw bytes, no decoding
```

### Constant vs Computed registers (data tables)

| Kind       | Lifetime       | Writable at runtime?                      | Use for                                     |
|------------|----------------|-------------------------------------------|---------------------------------------------|
| Constant   | Whole session  | NO (project-static; `tableSet` no-ops)    | Calibration coefficients, thresholds, gains |
| Computed   | One frame      | YES via `tableSet` (resets each frame)    | Cross-dataset rolling state, derived totals |

Computed registers reset at the START of every parsed frame, before
any transform runs. If you want state that survives across frames,
use a top-level `var` in your transform's IIFE — see `transforms`
skill.

### Schema version metadata (already in every saved project)

Every `.ssproj` file carries three root-level keys stamped by Serial
Studio at save time:

| Key                        | Meaning                                              |
|----------------------------|------------------------------------------------------|
| `schemaVersion`            | Project file format version (currently 1)            |
| `writerVersion`            | Serial Studio version that wrote this file           |
| `writerVersionAtCreation`  | Serial Studio version that originally created it     |

Use `project.getStatus` to see them on the loaded project. Older
Serial Studio versions ignore unknown keys, so a 3.4 project still
loads in 3.2 with any 3.4-only fields silently dropped.

### widgetType depends on context

Don't conflate these — they live in different namespaces:

| Where you see it             | Type                                              |
|------------------------------|---------------------------------------------------|
| `project.group.add{widgetType}` | GroupWidget enum (group SHAPE, e.g. DataGrid, MultiPlot) |
| `project.dataset.options` bit  | DatasetOption bitflag (per-dataset visualisation) |
| `project.workspace.addWidget{widgetType}` | DashboardWidget enum (the rendered tile) |

`dashboard_layout` skill has the full mapping; `api_semantics` has the
identity rules.

## Templates as starting points

For typed projects (IMU, GPS, scope, telemetry, MQTT subscriber), prefer
`project.template.apply{templateId}` over building from scratch. List
the catalog with `project.template.list`. After applying, narrate what
landed and offer to customize.

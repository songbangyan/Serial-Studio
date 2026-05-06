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

## Templates as starting points

For typed projects (IMU, GPS, scope, telemetry, MQTT subscriber), prefer
`project.template.apply{templateId}` over building from scratch. List
the catalog with `project.template.list`. After applying, narrate what
landed and offer to customize.

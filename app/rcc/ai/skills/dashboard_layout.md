# Dashboard Layout, Widgets, and Workspaces

Serial Studio's dashboard is a tabbed surface. Each tab is a "workspace"
holding a curated set of widgets pinned from your project's groups and
datasets.

## Widget catalog

Widgets are the things you see on the dashboard. They are NOT the same
as a group's `widget` string (that decides the group SHAPE, not what
tiles render). Use the `DashboardWidget` enum for
`project.workspace.addWidget`:

```
0  = DashboardTerminal           (DON'T pick this for tiles - it's the console)
1  = DashboardDataGrid
2  = DashboardMultiPlot
3  = DashboardAccelerometer
4  = DashboardGyroscope
5  = DashboardGPS
6  = DashboardPlot3D              (Pro)
7  = DashboardFFT
8  = DashboardLED
9  = DashboardPlot
10 = DashboardBar
11 = DashboardGauge
12 = DashboardCompass
13 = DashboardNoWidget            (sentinel; never pin this)
14 = DashboardImageView           (Pro)
15 = DashboardOutputPanel         (Pro; ALL output widgets in a group on one tile)
16 = DashboardNotificationLog     (Pro)
17 = DashboardWaterfall           (Pro)
18 = DashboardPainter             (Pro)
```

Each group/dataset is "compatible" with a subset of these. Use
`project.group.list` and read each group's `compatibleWidgetTypes`
array — the workspace-add command validates against it and rejects
mismatches.

## Three numbering systems — JSON key vs option bitflag vs widget enum

The same visualization choice shows up in three places with three
different names and numbers. Mixing them up is the most common source
of "I enabled it but the workspace still won't accept it" confusion.

| Visualization | JSON key (`.ssproj`) | `DatasetOption` bitflag (`setOption`) | `DashboardWidget` enum (`addWidget`) |
|---------------|----------------------|---------------------------------------|--------------------------------------|
| Plot          | `graph: true`        | `1`  (DatasetPlot)                    | `9`  (DashboardPlot)                 |
| FFT           | `fft: true`          | `2`  (DatasetFFT)                     | `7`  (DashboardFFT)                  |
| Bar           | `widget: "bar"`      | `4`  (DatasetBar)                     | `10` (DashboardBar)                  |
| Gauge         | `widget: "gauge"`    | `8`  (DatasetGauge)                   | `11` (DashboardGauge)                |
| Compass       | `widget: "compass"`  | `16` (DatasetCompass)                 | `12` (DashboardCompass)              |
| LED           | `led: true`          | `32` (DatasetLED)                     | `8`  (DashboardLED)                  |
| Waterfall     | `waterfall: true`    | `64` (DatasetWaterfall)               | `17` (DashboardWaterfall) (Pro)      |

Notes:
- Bar / Gauge / Compass are **mutually exclusive** — a dataset's `widget`
  string holds at most one of them. The `setOption` calls reflect that:
  enabling Bar clears Gauge and Compass on the same dataset.
- Plot / FFT / LED / Waterfall are **independent flags** — a dataset can
  have Plot + FFT + Waterfall all on at once, and the group's
  `compatibleWidgetTypes` will list all three.
- `compatibleWidgetTypes` is computed live from the dataset flags + the
  group's own `widget` string. Toggling a dataset option immediately
  expands or shrinks the group's compatible set; you do not need a
  separate group-level configuration.

## How to enable a workspace tile from scratch

If `addWidget` rejects your `widgetType` with "not compatible with group N":

1. Pick the right dataset in that group.
2. Look up the bitflag value from the table above (Plot is `1`, FFT is
   `2`, Waterfall is `64`).
3. `project.dataset.setOption{groupId, datasetId, option, enabled: true}`,
   OR `project.dataset.setOptions{groupId, datasetId, options: <bitflag>}`
   to set several at once,
   OR `project.dataset.update{groupId, datasetId, graph: true, fft: true,
   waterfall: true}` to set them inline alongside other field edits.
4. Re-run `addWidget` — `compatibleWidgetTypes` now includes the
   corresponding `DashboardWidget` enum.

## Workspaces

A workspace has an id (>= 1000), a title, an icon, and a list of widget
references. The user creates them via:

```
project.workspace.setCustomizeMode{enabled: true}    // required for edits
project.workspace.add{title, icon}                    // -> workspaceId
project.workspace.addWidget{workspaceId, widgetType, groupId,
                            relativeIndex: 0}
project.workspace.setCustomizeMode{enabled: false}    // optional
```

`relativeIndex` is **not** a dataset index. It's used only when you're
adding a SECOND tile of the same widgetType+groupId combination to the
same workspace. For all normal cases, leave it 0.

## Customize mode

Workspace mutations require `customizeWorkspaces = true`. You toggle
this with `project.workspace.setCustomizeMode{enabled}`. Without it, the
dashboard shows auto-generated workspaces; with it, the user's custom
layout takes over.

When you call any `project.workspace.add*`, `delete*`, `update`, or
`addWidget` and customize mode is off, the API returns
`validation_failed` with a hint to enable customize mode first.

## Auto-generation

`project.workspace.autoGenerate{}` materialises Serial Studio's default
auto-workspaces (one per group's natural widget type, loosely organised)
into the customised list. It's a one-shot — call once for users who want
"a reasonable starting layout", then iterate.

## Building an executive / overview dashboard

When the user asks for "an overview" or "executive dashboard":

1. `project.group.list` — read every group's `datasetSummary` and
   `compatibleWidgetTypes`.
2. Pick 4–8 groups whose data is genuinely summary-relevant: speed, RPM,
   temperature, fuel, voltage, state-of-charge, primary alarms. SKIP
   raw-flag groups (door open, individual lights, individual brake
   pressures) — those belong on dedicated diagnostic workspaces.
3. For each pick, choose the most readable widgetType from
   compatibleWidgetTypes:
   - Gauge (11) for single scalars with min/max
   - MultiPlot (2) for related time-series
   - DataGrid (1) for tabular reads
   - Compass (12) for headings
   - Bar (10) for bounded levels
   - LED (8) for booleans/alarms
   - Plot (9) for single time-series
   - FFT (7) for spectra of audio / vibration / signals
   - Waterfall (17, Pro) for spectrograms
4. NEVER widgetType=0 (Terminal).
5. Show the user the curated list in chat BEFORE pushing. Let them
   confirm or redirect.
6. `setCustomizeMode{enabled: true}`, `workspace.add{title: 'Overview',
   icon: 'qrc:/icons/panes/overview.svg'}` (always provide an icon),
   then `addWidget` for each pick.

## Recipe — Plot + FFT + Waterfall on the same dataset

Goal: one workspace tab that shows the time-domain signal, its FFT, and
a waterfall (Pro) for the same audio/vibration channel.

```
// 1. Find the dataset's groupId and datasetId.
project.group.list                 -> note groupId for the channel
project.dataset.list               -> note datasetId within that group

// 2. Enable all three visualizations on the dataset in one shot.
project.dataset.setOptions {
  groupId: <gid>,
  datasetId: <did>,
  options: 67                      // 1 (Plot) | 2 (FFT) | 64 (Waterfall)
}

// 3. Customize mode is required for any workspace edit.
project.workspace.setCustomizeMode { enabled: true }

// 4. Create the workspace.
project.workspace.add { title: "Signal Analysis",
                        icon: "qrc:/icons/panes/dashboard.svg" }
// -> { id: <wsId> }

// 5. Pin all three tiles. relativeIndex is 0 for each — it's only > 0
//    when adding a second tile of the same widgetType+groupId.
project.workspace.addWidget { workspaceId: <wsId>, widgetType: 9,
                              groupId: <gid>, relativeIndex: 0 }   // Plot
project.workspace.addWidget { workspaceId: <wsId>, widgetType: 7,
                              groupId: <gid>, relativeIndex: 0 }   // FFT
project.workspace.addWidget { workspaceId: <wsId>, widgetType: 17,
                              groupId: <gid>, relativeIndex: 0 }   // Waterfall
```

Common mistake: setting `widgetType: 1` (Plot bitflag) instead of
`widgetType: 9` (DashboardPlot enum). The bitflag is only for
`setOption` / `setOptions`; `addWidget` always takes the
`DashboardWidget` enum.

## Common gotchas

- **Widget IDs ≥ 1000 = workspaces, < 1000 = groups.** Don't cross-wire.
- **icon is required** in practice. Without an icon path, the taskbar
  tile renders blank. Use a `qrc:/icons/...` path; the
  `panes/overview.svg`, `panes/dashboard.svg`, `panes/setup.svg`,
  `panes/console.svg` are always available.
- **widgetType=0** is the Terminal/Console widget. Almost never what the
  user wants on a workspace tile.
- **Customize mode persists**. If you turn it on and the user closes the
  app, it stays on. That's fine, but be aware that subsequent
  auto-generate calls become no-ops.

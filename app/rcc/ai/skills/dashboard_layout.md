# Dashboard Layout, Widgets, and Workspaces

Serial Studio's dashboard is a tabbed surface. Each tab is a "workspace"
holding a curated set of widgets pinned from your project's groups and
datasets.

## Widget catalog

Widgets are the things you see on the dashboard. They are NOT the same
as a group's `widgetType` (that decides the group SHAPE, not what tiles
render). Use the `DashboardWidget` enum for `project.workspace.addWidget`:

```
0  = DashboardTerminal      (DON'T pick this for tiles - it's the console)
1  = DashboardDataGrid
2  = DashboardMultiPlot
3  = DashboardFFT
4  = DashboardGPS
5  = DashboardAccelerometer
6  = DashboardGyroscope
7  = DashboardCompass
8  = DashboardLED
9  = DashboardPlot
10 = DashboardBar
11 = DashboardGauge
12 = (reserved)
13 = DashboardImageView           (Pro)
14 = DashboardPlot3D              (Pro)
15 = DashboardOutputPanel         (Pro; ALL output widgets in a group on one tile)
16 = DashboardWaterfall           (Pro)
17 = (reserved)
18 = DashboardPainter             (Pro)
```

Each group/dataset is "compatible" with a subset of these. Use
`project.group.list` and read each group's `compatibleWidgetTypes`
array — the workspace-add command validates against it and rejects
mismatches.

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
   - Compass (7) for headings
   - Bar (10) for bounded levels
   - LED (8) for booleans/alarms
4. NEVER widgetType=0 (Terminal).
5. Show the user the curated list in chat BEFORE pushing. Let them
   confirm or redirect.
6. `setCustomizeMode{enabled: true}`, `workspace.add{title: 'Overview',
   icon: 'qrc:/icons/panes/overview.svg'}` (always provide an icon),
   then `addWidget` for each pick.

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

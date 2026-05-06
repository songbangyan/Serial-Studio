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

## Per-dataset visualization options — the bitflag table

A **dataset** carries its own visualization options as bit flags. Each
flag toggles ONE per-dataset visualization. The same visualization shows
up in three places with three different names and numbers — keep this
table in front of you whenever you're enabling/disabling visualizations:

| Visualization | JSON key (`.ssproj`) | `DatasetOption` bitflag (`setOptions`) | `DashboardWidget` enum (`addWidget`) |
|---------------|----------------------|----------------------------------------|--------------------------------------|
| Plot          | `graph: true`        | `1`  (DatasetPlot)                    | `9`  (DashboardPlot)                 |
| FFT           | `fft: true`          | `2`  (DatasetFFT)                     | `7`  (DashboardFFT)                  |
| Bar           | `widget: "bar"`      | `4`  (DatasetBar)                     | `10` (DashboardBar)                  |
| Gauge         | `widget: "gauge"`    | `8`  (DatasetGauge)                   | `11` (DashboardGauge)                |
| Compass       | `widget: "compass"`  | `16` (DatasetCompass)                 | `12` (DashboardCompass)              |
| LED           | `led: true`          | `32` (DatasetLED)                     | `8`  (DashboardLED)                  |
| Waterfall     | `waterfall: true`    | `64` (DatasetWaterfall)               | `17` (DashboardWaterfall) (Pro)      |

Notes:
- Bar / Gauge / Compass are **mutually exclusive** — a dataset's `widget`
  string holds at most one of them. `setOptions` enforces this: if more
  than one of those bits is set in the value, the highest bit wins.
- Plot / FFT / LED / Waterfall are **independent flags** — a dataset can
  have Plot + FFT + Waterfall all on at once, and the group's
  `compatibleWidgetTypes` will list all three.
- The **DatasetOption bitflag and the DashboardWidget enum are
  different numbering systems.** Plot is bit `1` for `setOptions` but
  enum value `9` for `addWidget`. Bar is bit `4` but enum `10`.
  Hardcoding `widgetType: 1` in `addWidget` thinking it means "Plot"
  is the most common bug — that's `DashboardDataGrid`. Always read
  the column you actually need.
- `compatibleWidgetTypes` is computed live from the dataset flags + the
  group's own widget shape (see next section). Toggling a dataset
  option immediately expands or shrinks the group's compatible set.

## Group widget shape — separate from per-dataset flags

A **group** also carries its own widget shape, which determines the
GROUP-level tile (DataGrid, MultiPlot, Accelerometer, GPS, Painter,
…). This is independent of the per-dataset bitflags above and uses a
DIFFERENT enum: `GroupWidget`.

| GroupWidget int | Group shape    | Resulting `DashboardWidget` enum |
|-----------------|----------------|----------------------------------|
| `0`             | DataGrid       | `1`  (DashboardDataGrid)         |
| `1`             | Accelerometer  | `3`  (DashboardAccelerometer)    |
| `2`             | Gyroscope      | `4`  (DashboardGyroscope)        |
| `3`             | GPS            | `5`  (DashboardGPS)              |
| `4`             | MultiPlot      | `2`  (DashboardMultiPlot)        |
| `5`             | NoGroupWidget  | (none — group has no native tile)|
| `6`             | Plot3D (Pro)   | `6`  (DashboardPlot3D)           |
| `7`             | ImageView (Pro)| `14` (DashboardImageView)        |
| `8`             | Painter (Pro)  | `18` (DashboardPainter)          |

Reading the API:
- `project.group.add{widgetType: <int>}` — REQUIRED at creation; this
  is the GroupWidget enum (the int column above).
- `project.group.update` accepts `{title, widget, columns, sourceId,
  painterCode}`. **It does NOT accept `widgetType`.** Pass `widget` as
  a STRING ("datagrid", "multiplot", "accelerometer", "gyro", "map",
  "plot3d", "image", "painter") or `""` to clear. Sending
  `widgetType: 4` to `update` is silently dropped — you'll see no
  error, and `compatibleWidgetTypes` won't change. If you wrote that
  call and saw nothing happen, that's why.
- The group's `compatibleWidgetTypes` array is the **union** of:
  - the group-shape DashboardWidget (right column above), if any, AND
  - every per-dataset DashboardWidget enabled by the bit flags in the
    previous table, across all datasets in the group.

So `compatibleWidgetTypes` is *derived state*, not configuration. You
never write to it directly; you write to the inputs (group widget +
dataset options) and read the result back from `project.group.list`.

## How to enable a workspace tile from scratch

If `addWidget` rejects your `widgetType` with "not compatible with group N":

1. Pick the right dataset in that group.
2. Look up the bitflag value from the per-dataset table above (Plot
   is `1`, FFT is `2`, Gauge is `8`, Waterfall is `64`).
3. `project.dataset.setOptions{groupId, datasetId, options: <bitfield>}`
   is the canonical call. Pass the bitwise OR of every flag you want
   enabled (any flag NOT in the value is **disabled**, so include the
   ones already on). `project.dataset.update{..., graph: true, fft:
   true, waterfall: true}` is the alternative when you're patching
   other dataset fields (title, units, ranges) in the same call.
   `project.dataset.setOption` (singular) is **deprecated** — it
   silently corrupts state when the AI repeatedly toggles single
   bits and forgets the rest. Don't use it from agent code; use
   `setOptions` (plural) and recompute the bitfield each time.
4. **VERIFY**: re-run `project.group.list` and read the target group's
   `compatibleWidgetTypes`. The new DashboardWidget enum value MUST
   appear in that list. If it doesn't, your `setOptions` call did not
   land — check the `enabledWidgetTypes` of every dataset in that
   group, the bit you flipped, and that you used the right (groupId,
   datasetId) pair.
5. Now re-run `addWidget`.

**Never call `addWidget` twice with identical args expecting a
different result.** If a call failed, something must change between
attempts (different widgetType, different groupId, or you flipped a
dataset option AND verified via `project.group.list`). Looping the
same call wastes turns and signals to the user that you're not
reading errors.

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

## MANDATORY pre-flight before any workspace edit

Trial-and-error against `addWidget` burns calls on validation errors and
leaves the workspace half-built. ALWAYS run this exact sequence first:

```
1. project.group.list       -> for every group, read groupId,
                                compatibleWidgetTypes, and
                                datasetSummary[].enabledWidgetTypes.
                                READ THE ARRAY -- don't assume what's in it.
2. project.workspace.list   -> read existing workspaceIds (the ones
                                you may need to delete or reuse)
3. (optional, if a command schema is unfamiliar)
   meta.describeCommand{name: "project.workspace.addWidget"}
4. NOW you can plan and execute.
```

The plan is: for each widget the user asked for, pick a `groupId` whose
`compatibleWidgetTypes` contains the desired `widgetType`. If the
desired type isn't in any group's compatible list, FIRST flip the
matching dataset option (see "How to enable a workspace tile from
scratch" above), THEN re-read `project.group.list` to confirm the type
is now compatible, THEN call `addWidget`.

`relativeIndex` is almost always `0`. Only use a non-zero value when
you are intentionally pinning a SECOND tile of the same `(widgetType,
groupId)` pair to the same workspace. **It is NOT a dataset index.**
Passing a dataset index here is a noise-free bug — you'll get a tile
that overlaps an existing one, or no tile at all.

## Verify-after-mutation rule

Every project mutation in this skill (group widget shape, dataset
options, workspace add/delete) **changes derived state** the next call
will validate against. The validators don't lie, but they return JSON
errors — they don't apologize.

After ANY mutation that could affect `compatibleWidgetTypes`, before
the next `addWidget` call, re-run `project.group.list` and read the
relevant group. If the array doesn't contain the widgetType you're
about to pass, your mutation didn't do what you thought. Stop, read
the result, and figure out why **before** issuing another addWidget.

Mutations that affect `compatibleWidgetTypes`:
- `project.dataset.setOptions` / `setOption` / `update{graph, fft,
  led, waterfall, widget}`
- `project.dataset.add` (new datasets seed the union)
- `project.dataset.delete` (shrinks the union)
- `project.group.update{widget: "..."}`  (string, see next bullet)
- `project.group.add{widgetType: <int>}`  (GroupWidget enum int)

What does NOT affect it (silent no-op when used wrongly):
- `project.group.update{widgetType: ...}` — `update` does not accept
  `widgetType`. Use `widget` (string) instead.

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
`setOptions`; `addWidget` always takes the `DashboardWidget` enum.

## Recipe — Add a Gauge tile to an existing group

Goal: a group has one numeric dataset that you want to show as a radial
gauge on the executive workspace.

```
// 1. Pre-flight: identify groupId, current dataset options, and
//    target workspace.
project.group.list
  -> note groupId (e.g. 0)
  -> note compatibleWidgetTypes  (e.g. [9] = Plot only)
  -> note datasetSummary[0]      (e.g. datasetId=0, options bit 1 = Plot on)
project.workspace.list
  -> note workspaceId for "Executive Overview" (e.g. 5001)

// 2. Gauge isn't in compatibleWidgetTypes yet. Flip Gauge (bit 8) on the
//    dataset, preserving Plot (bit 1) -- pass the OR of every flag you
//    want enabled.
project.dataset.setOptions {
  groupId: 0,
  datasetId: 0,
  options: 9                         // 1 (Plot) | 8 (Gauge)
}

// 3. VERIFY. Re-list groups; compatibleWidgetTypes must now include 11.
project.group.list
  -> compatibleWidgetTypes: [9, 11]  -- good, addWidget will accept 11

// 4. Pin the Gauge tile.
project.workspace.setCustomizeMode { enabled: true }
project.workspace.addWidget {
  workspaceId: 5001,
  widgetType: 11,                    // DashboardGauge -- NOT 8 (that's bit value)
  groupId: 0,
  relativeIndex: 0
}

// 5. Save.
project.save
```

If step 4 returns "widgetType=11 not compatible with group 0", step 3
didn't actually pass — re-list groups and check what
`compatibleWidgetTypes` really contains, instead of issuing addWidget
again.

## Troubleshooting — what the API errors actually mean

| Error message | What happened | Fix |
|---------------|---------------|-----|
| `widgetType=0 is DashboardTerminal, not a workspace tile.` | You passed `widgetType: 0` to `addWidget`. Often a swapped-args bug (groupId of 0 ended up in the widgetType slot, or you pulled an int from the wrong field of a JSON result). | Re-check your call. The schema is `{workspaceId, widgetType, groupId, relativeIndex}` — `groupId=0` is valid, `widgetType=0` is not. |
| `widgetType=N not compatible with group M.` | Group M's `compatibleWidgetTypes` doesn't contain N. | Either pick a different widgetType from M's list, or enable the corresponding bit on a dataset in M (see the "How to enable a workspace tile from scratch" recipe), then RE-LIST and verify before retrying. |
| Group widget didn't change after `project.group.update {widgetType: ...}` | `group.update` doesn't have a `widgetType` field. The param was silently ignored. | Use `project.group.update {widget: "multiplot"}` (string), or accept that the shape is locked at `add` time and `delete + add` if you really need to change it. |
| `addWidget` succeeded but no tile appears on the dashboard | `customizeWorkspaces` mode is off, OR you pinned to the wrong workspaceId, OR the workspace title bar is filtered. | Confirm `setCustomizeMode {enabled: true}` ran successfully and the user is looking at the correct workspace tab. |
| Same call keeps failing | The state hasn't changed between calls. | Read the error, read `project.group.list`, find the actual cause. Do NOT issue the same call a third time hoping for a different result. |

## Common gotchas

- **Widget IDs ≥ 1000 = workspaces, < 1000 = groups.** Don't cross-wire.
- **icon is required** in practice. Without an icon path, the taskbar
  tile renders blank. Use a `qrc:/icons/...` path; the
  `panes/overview.svg`, `panes/dashboard.svg`, `panes/setup.svg`,
  `panes/console.svg` are always available.
- **widgetType=0** is the Terminal/Console widget. Almost never what the
  user wants on a workspace tile.
- **`group.add` takes `widgetType` (int); `group.update` takes
  `widget` (string).** Different fields, different types. `update`
  silently drops `widgetType`.
- **`setOption` (singular) is deprecated for agent use.** Always
  `setOptions` (plural) with the full bitfield, or `dataset.update`
  with named booleans (`graph`, `fft`, `led`, `waterfall`).
- **Customize mode persists**. If you turn it on and the user closes the
  app, it stays on. That's fine, but be aware that subsequent
  auto-generate calls become no-ops.

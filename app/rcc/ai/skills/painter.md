# Painter Widget

The Painter widget (Pro) gives you a Canvas-2D drawing surface. One per
group with `widgetType: 8`. Bind code via
`project.painter.setCode{groupId, code}`.

## Decision: when to use a painter

- **Don't** for standard widgets (gauge, plot, bar, LED, compass) — those
  are dataset options, not painters. Painter is one or two orders of
  magnitude more code.
- **Do** for visualizations the standard widgets can't express: status
  grids combining many datasets, custom dial faces, schematics with live
  values, oscilloscope-style traces, polar/radar plots, vector fields,
  segmented displays, pixel-pushed image outputs.

A painter group can be **empty** (zero datasets). Read peer datasets via
`datasetGetFinal(uniqueId)` instead of duplicating data into the painter
group.

## Entry points

- **`paint(ctx, w, h)` — REQUIRED.** Called every UI tick (24 Hz default)
  to redraw the canvas. The function name is `paint`, not `draw`, not
  `render`. The engine looks up `globalThis.paint` by name.
- **`onFrame()` — optional.** Called once per parsed frame, before the
  next paint, with no arguments. Cache state in top-level `var`s.

`bootstrap()` does NOT exist. Top-level statements at the script's outer
scope run once when the script compiles — that is your bootstrap.

## Iteration workflow

1. Get current code: `project.painter.getCode{groupId}` (returns empty
   string if the group has no painter set yet).
2. Get the peer dataset uniqueIds you'll read:
   `project.dataset.list` returns `uniqueId` per dataset.
3. Read tables you'll reference: `project.dataTable.list` then
   `project.dataTable.get{name}` for each.
4. Read scripting reference: `meta.fetchScriptingDocs{kind: "painter_js"}`.
5. Adapt a real example: `scripts.list{kind: "painter"}` then
   `scripts.get{kind: "painter", id: "<closest match>"}`.
6. Dry-run: `project.painter.dryRun{code}` verifies compile + that
   `paint(ctx, w, h)` is defined. Doesn't render — runtime errors inside
   `paint` only surface when the live widget mounts.
7. Push: `project.painter.setCode{groupId, code}`.

## Globals you can use

```js
datasets[i]      // proxy; .length, .value, .rawValue, .title, .units,
                 // .uniqueId, .min, .max, .widgetMin/Max, etc.
group            // .id, .title, .columns, .sourceId
frame            // .number, .timestampMs
theme            // ThemeManager palette; widget_base, widget_border,
                 // widget_text, widget_highlight, alarm, etc.
                 // theme.widget_colors is an array of per-channel colors.
console          // log/warn/error to the editor status pane
tableGet, tableSet, datasetGetRaw, datasetGetFinal
```

## Color from theme

Don't hard-code hex. Use `theme.widget_base` for background,
`theme.widget_border` for grid/frame, `theme.widget_text` for labels,
`theme.widget_highlight` for the primary signal, `theme.widget_colors[i]`
for per-channel colors, `theme.alarm` for red/critical states. The
canvas tracks light/dark theme switches automatically.

## arc / moveTo discipline

The Painter renderer requires `moveTo` BEFORE `arc` to start a new
subpath. Without `moveTo`, the arc is implicitly connected to the
previous path endpoint and may render a stray chord:

```js
ctx.beginPath();
ctx.moveTo(cx + r, cy);          // explicit start
ctx.arc(cx, cy, r, 0, Math.PI);  // arc opens cleanly
ctx.stroke();
```

## Canvas subset

Pen/fill: `strokeStyle`, `fillStyle`, `lineWidth`, `lineCap`, `lineJoin`,
`miterLimit`, `setLineDash`.
Path: `beginPath`, `moveTo`, `lineTo`, `arc`, `arcTo`, `quadraticCurveTo`,
`bezierCurveTo`, `closePath`.
Fill/stroke: `fill`, `stroke`, `clearRect`, `fillRect`, `strokeRect`.
Transforms: `save`, `restore`, `translate`, `rotate`, `scale`,
`setTransform`, `resetTransform`.
Text: `font`, `textAlign`, `textBaseline`, `fillText`, `strokeText`,
`measureText` (returns `{width, ascent, descent, ...}`),
`measureTextWidth` (returns just a number).
Gradients: `createLinearGradient`, `createRadialGradient`,
`gradient.addColorStop`.

NOT supported: `drawImage` from URLs, `createPattern`, `putImageData`,
`getImageData`.

## Performance

Paint runs at 24 Hz. Cheap: `lineTo`, `arc`, `fillText`, arithmetic.
Avoid: `JSON.stringify`, allocating arrays per frame, creating new
gradients per call (cache in a top-level `var`).

Throwing inside `paint` logs a watchdog warning and the canvas keeps the
last successful frame on screen.

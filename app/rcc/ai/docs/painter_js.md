# Painter Widget API — JavaScript

The Painter widget (Pro) gives you a Canvas-style 2D drawing surface bound
to a single dataset value. You author a script with two hooks:

- `bootstrap()` — runs once at widget load. Define top-level `var`s here,
  cache styles, precompute layout. **No** `Object.defineProperty` lockdown
  on top-level variables — the engine recompiles the script on edit and
  needs to reassign them.
- `draw(ctx, value)` — called every UI tick (24 Hz default). `value` is
  the current dataset value. `ctx` is a Canvas-2D-like context.

## Bootstrap rules

- Top-level `var` is preserved across `draw` calls; that's how you keep
  state between frames.
- Don't seal globals with `Object.defineProperty`/`Object.freeze` — script
  recompilation will throw on the second edit.
- Errors during `bootstrap` are surfaced in the editor's status pane;
  fix and re-apply.

```js
var width, height, cx, cy;

function bootstrap() {
  width = ctx.canvas.width;
  height = ctx.canvas.height;
  cx = width / 2;
  cy = height / 2;
}
```

## Canvas context

The injected `ctx` supports the standard Canvas-2D API plus a few
restrictions:

- Pen / fill: `strokeStyle`, `fillStyle`, `lineWidth`, `lineCap`,
  `lineJoin`, `miterLimit`, `setLineDash`.
- Path: `beginPath`, `moveTo`, `lineTo`, `arc`, `arcTo`, `quadraticCurveTo`,
  `bezierCurveTo`, `closePath`.
- Fill / stroke: `fill`, `stroke`, `clearRect`, `fillRect`, `strokeRect`.
- Transforms: `save`, `restore`, `translate`, `rotate`, `scale`,
  `setTransform`, `resetTransform`.
- Text: `font`, `textAlign`, `textBaseline`, `fillText`, `strokeText`,
  `measureText`.
- Gradients: `createLinearGradient`, `createRadialGradient`,
  `addColorStop`.

Not supported: `drawImage` from URLs (no network), `createPattern`,
`putImageData`, `getImageData`.

## arc / moveTo discipline

The Painter renderer requires `moveTo` **before** `arc` whenever you need
the arc to start a new subpath. Without `moveTo`, the arc is implicitly
connected to the previous path endpoint and may render a stray chord:

```js
ctx.beginPath();
ctx.moveTo(cx + r, cy);          // explicit start point
ctx.arc(cx, cy, r, 0, Math.PI);  // arc opens cleanly
ctx.stroke();
```

For full circles, the order is the same — `moveTo` to the perimeter, then
`arc` to `2*Math.PI`.

## Color from theme

Hard-coded hex colors look fine in light mode and break dark mode. Use the
injected theme palette:

```js
ctx.strokeStyle = theme.text;
ctx.fillStyle   = theme.window;
```

`theme` exposes the same keys as `Cpp_ThemeManager.colors[*]`:
`text, window, base, button, button_text, highlight, accent, mid,
groupbox_background, groupbox_border, ...`.

## Example: light-themed dial gauge

```js
var width, height, cx, cy, r;
var lastValue = 0;

function bootstrap() {
  width  = ctx.canvas.width;
  height = ctx.canvas.height;
  cx     = width / 2;
  cy     = height * 0.55;
  r      = Math.min(width, height) * 0.40;
}

function draw(ctx, value) {
  // Background
  ctx.clearRect(0, 0, width, height);

  // Outer arc (always full)
  ctx.beginPath();
  ctx.moveTo(cx + r, cy);
  ctx.arc(cx, cy, r, 0, 2 * Math.PI);
  ctx.lineWidth = 2;
  ctx.strokeStyle = theme.groupbox_border;
  ctx.stroke();

  // Filled arc (0..value, mapped to 0..pi)
  var pct = Math.max(0, Math.min(1, value / 100));
  ctx.beginPath();
  ctx.moveTo(cx + r, cy);
  ctx.arc(cx, cy, r, 0, Math.PI * pct);
  ctx.lineWidth = 6;
  ctx.strokeStyle = theme.highlight;
  ctx.stroke();

  // Centered numeric value
  ctx.fillStyle = theme.text;
  ctx.font = '20px sans-serif';
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText(value.toFixed(1), cx, cy + r * 0.10);

  lastValue = value;
}
```

## Performance

`draw` runs at 24 Hz by default. Cheap: lineTo / arc / fillText calls,
arithmetic. Avoid: building intermediate strings every frame
(`toFixed(1)` is fine; `JSON.stringify` is not), creating new gradients
per call (cache in `bootstrap`).

## Errors

Throwing inside `draw` logs a watchdog warning and the canvas keeps the
last successful frame on screen. The error surfaces in the editor's
status pane.

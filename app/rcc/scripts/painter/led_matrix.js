// LED matrix.
//
// Each dataset becomes a row of LEDs. The number of LEDs per row scales with
// the value relative to (min, max). Useful for VU-style level meters and
// discrete-state indicators.

const LEDS_PER_ROW = 16;
const ON_COLOR     = "#22c55e";
const WARN_COLOR   = "#f59e0b";
const ERR_COLOR    = "#ef4444";

function paint(ctx, w, h) {
  ctx.fillStyle = "#0a0a0c";
  ctx.fillRect(0, 0, w, h);

  if (datasets.length === 0)
    return;

  const padX     = 16;
  const padY     = 12;
  const labelW   = 80;
  const rowH     = (h - padY * 2) / datasets.length;
  const ledH     = Math.min(rowH * 0.6, 18);
  const ledArea  = w - padX * 2 - labelW - 8;
  const ledW     = ledArea / LEDS_PER_ROW - 2;

  for (let i = 0; i < datasets.length; ++i) {
    const ds   = datasets[i];
    const v    = ds.value;
    const norm = Number.isFinite(v) ? Math.max(0, Math.min(1, (v - ds.min) / (ds.max - ds.min || 1))) : 0;
    const lit  = Math.round(norm * LEDS_PER_ROW);
    const yMid = padY + i * rowH + rowH * 0.5;

    ctx.fillStyle    = "#cbd5e1";
    ctx.font         = "12px monospace";
    ctx.textBaseline = "middle";
    ctx.fillText(ds.title.substring(0, 12), padX, yMid);

    for (let k = 0; k < LEDS_PER_ROW; ++k) {
      const x = padX + labelW + 8 + k * (ledW + 2);
      const y = yMid - ledH * 0.5;

      if (k < lit) {
        const t = k / LEDS_PER_ROW;
        ctx.fillStyle = t < 0.66 ? ON_COLOR : (t < 0.85 ? WARN_COLOR : ERR_COLOR);
      } else {
        ctx.fillStyle = "#1f2937";
      }
      ctx.fillRect(x, y, ledW, ledH);
    }
  }
  ctx.textBaseline = "alphabetic";
}

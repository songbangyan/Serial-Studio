// Status grid.
//
// Renders each dataset as a labelled tile colored by alarm thresholds:
// green when in range, amber on alarmLow/alarmHigh boundary, red outside.

function pickColor(ds) {
  const v = ds.value;
  if (!Number.isFinite(v))
    return "#475569";

  if (v < ds.alarmLow || v > ds.alarmHigh)
    return "#dc2626";

  const pad = (ds.alarmHigh - ds.alarmLow) * 0.1;
  if (v < ds.alarmLow + pad || v > ds.alarmHigh - pad)
    return "#f59e0b";

  return "#16a34a";
}

function paint(ctx, w, h) {
  ctx.fillStyle = "#0b1220";
  ctx.fillRect(0, 0, w, h);

  const n = datasets.length;
  if (n === 0)
    return;

  const cols = Math.max(1, group.columns || 2);
  const rows = Math.ceil(n / cols);
  const gap  = 6;
  const cellW = (w - gap * (cols + 1)) / cols;
  const cellH = (h - gap * (rows + 1)) / rows;

  for (let i = 0; i < n; ++i) {
    const r = Math.floor(i / cols);
    const c = i % cols;
    const x = gap + c * (cellW + gap);
    const y = gap + r * (cellH + gap);

    ctx.fillStyle = pickColor(datasets[i]);
    ctx.fillRect(x, y, cellW, cellH);

    ctx.fillStyle  = "#0b1220";
    ctx.font       = "bold 14px sans-serif";
    ctx.textAlign  = "center";
    ctx.textBaseline = "middle";
    const label    = datasets[i].title;
    const value    = datasets[i].value.toFixed(2) + " " + datasets[i].units;

    ctx.fillText(label, x + cellW * 0.5, y + cellH * 0.40);
    ctx.font = "12px monospace";
    ctx.fillText(value, x + cellW * 0.5, y + cellH * 0.65);
    ctx.textAlign = "start";
    ctx.textBaseline = "alphabetic";
  }
}

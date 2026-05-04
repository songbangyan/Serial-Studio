// Status grid.
//
// Each dataset becomes a labelled card whose accent strip is coloured by
// the alarm thresholds: green when in range, amber when in the boundary
// zone, red when out of range, slate when the value isn't finite.

function status_color(ds) {
  const v = ds.value;
  if (!Number.isFinite(v)) return { tag: "no-data", color: "#94a3b8" };

  const lo = ds.alarmLow;
  const hi = ds.alarmHigh;
  if (Number.isFinite(lo) && Number.isFinite(hi) && hi > lo) {
    if (v < lo || v > hi) return { tag: "alarm",   color: "#dc2626" };
    const pad = (hi - lo) * 0.10;
    if (v < lo + pad || v > hi - pad) return { tag: "warn", color: "#f59e0b" };
    return { tag: "ok", color: "#10b981" };
  }
  return { tag: "info", color: "#0ea5e9" };
}

function paint(ctx, w, h) {
  // Cream paper background.
  ctx.fillStyle = "#f5f5f1";
  ctx.fillRect(0, 0, w, h);
  ctx.strokeStyle = "#e7e5de";
  ctx.lineWidth = 2;
  ctx.strokeRect(1, 1, w - 2, h - 2);

  const n = datasets.length;
  if (n === 0) return;

  // Header.
  const headerH = 22;
  ctx.fillStyle = "#0f172a";
  ctx.font = "bold 11px sans-serif";
  ctx.textAlign = "start";
  ctx.fillText("STATUS  GRID", 14, 16);
  ctx.fillStyle = "#64748b";
  ctx.font = "10px sans-serif";
  ctx.textAlign = "end";
  ctx.fillText(n + " channels", w - 14, 16);
  ctx.fillStyle = "#e5e7eb";
  ctx.fillRect(14, 20, w - 28, 1);

  const cols = Math.max(1, group.columns || Math.min(n, Math.max(1, Math.round(Math.sqrt(n * w / (h - headerH))))));
  const rows = Math.ceil(n / cols);
  const padX = 14;
  const padY = headerH + 6;
  const gap  = 8;
  const cellW = (w - padX * 2 - gap * (cols - 1)) / cols;
  const cellH = (h - padY - padX - gap * (rows - 1)) / rows;

  for (let i = 0; i < n; ++i) {
    const r = Math.floor(i / cols);
    const c = i % cols;
    const x = padX + c * (cellW + gap);
    const y = padY + r * (cellH + gap);
    const ds = datasets[i];
    const status = status_color(ds);

    // Card with shadow.
    ctx.fillStyle = "#e2e8f0";
    ctx.fillRect(x + 1, y + 2, cellW, cellH);
    ctx.fillStyle = "#ffffff";
    ctx.fillRect(x, y, cellW, cellH);
    ctx.strokeStyle = "#d4d4d8";
    ctx.lineWidth = 1;
    ctx.strokeRect(x + 0.5, y + 0.5, cellW - 1, cellH - 1);

    // Coloured accent strip on the left edge.
    ctx.fillStyle = status.color;
    ctx.fillRect(x, y, 4, cellH);

    // Status pill in the top-right corner.
    const pillW = 38;
    const pillH = 14;
    ctx.fillStyle = status.color;
    ctx.fillRect(x + cellW - pillW - 6, y + 6, pillW, pillH);
    ctx.fillStyle = "#ffffff";
    ctx.font = "bold 9px sans-serif";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText(status.tag.toUpperCase(), x + cellW - pillW * 0.5 - 6, y + 6 + pillH * 0.5);

    // Title (truncated).
    ctx.fillStyle = "#475569";
    ctx.font = "10px sans-serif";
    ctx.textAlign = "start";
    ctx.textBaseline = "alphabetic";
    const title = (ds.title || "").substring(0, 28);
    ctx.fillText(title, x + 12, y + 24);

    // Value (large, bold). Measure width with the value's own font so the
    // units label lands flush to the right of the number.
    ctx.fillStyle = "#0f172a";
    ctx.font = "bold 18px sans-serif";
    const value = Number.isFinite(ds.value)
      ? ds.value.toFixed(2)
      : "--";
    const valueW = ctx.measureTextWidth(value);
    ctx.fillText(value, x + 12, y + 50);

    if (ds.units) {
      ctx.fillStyle = "#64748b";
      ctx.font = "10px sans-serif";
      ctx.fillText(ds.units, x + 12 + valueW + 6, y + 50);
    }
  }

  ctx.textAlign = "start";
  ctx.textBaseline = "alphabetic";
}

// Round dial gauge with coloured arc.
//
// Renders datasets[0] as an analog gauge with major/minor ticks. Falls back
// to a useful display even when min/max are zero.

function paint(ctx, w, h) {
  ctx.fillStyle = "#0a0a0c";
  ctx.fillRect(0, 0, w, h);

  if (datasets.length === 0) return;

  const ds  = datasets[0];
  const v   = Number.isFinite(ds.value) ? ds.value : 0;
  const lo  = ds.min;
  const hi  = (ds.max > ds.min) ? ds.max : (ds.min + 100);
  const cx  = w * 0.5;
  const cy  = h * 0.55;
  const r   = Math.min(w, h * 1.4) * 0.42;

  const startA = Math.PI * 0.75;
  const endA   = Math.PI * 2.25;
  const sweep  = endA - startA;

  ctx.strokeStyle = "#1f2937";
  ctx.lineWidth   = 16;
  ctx.lineCap     = "round";
  ctx.beginPath();
  ctx.arc(cx, cy, r, startA, endA);
  ctx.stroke();

  const norm = Math.max(0, Math.min(1, (v - lo) / (hi - lo)));
  const valA = startA + sweep * norm;

  let arcColor = "#22c55e";
  if (Number.isFinite(ds.alarmHigh) && v > ds.alarmHigh) arcColor = "#ef4444";
  else if (Number.isFinite(ds.alarmLow) && v < ds.alarmLow) arcColor = "#3b82f6";
  else if (norm > 0.85) arcColor = "#f59e0b";

  ctx.strokeStyle = arcColor;
  ctx.beginPath();
  ctx.arc(cx, cy, r, startA, valA);
  ctx.stroke();
  ctx.lineCap = "butt";

  for (let i = 0; i <= 10; ++i) {
    const a    = startA + (sweep / 10) * i;
    const inner = r - (i % 5 === 0 ? 22 : 12);
    ctx.strokeStyle = "#94a3b8";
    ctx.lineWidth   = i % 5 === 0 ? 2 : 1;
    ctx.beginPath();
    ctx.moveTo(cx + Math.cos(a) * inner, cy + Math.sin(a) * inner);
    ctx.lineTo(cx + Math.cos(a) * (r - 30), cy + Math.sin(a) * (r - 30));
    ctx.stroke();

    if (i % 5 === 0) {
      const tick = lo + ((hi - lo) / 10) * i;
      const tx   = cx + Math.cos(a) * (r - 38);
      const ty   = cy + Math.sin(a) * (r - 38);
      ctx.fillStyle    = "#cbd5e1";
      ctx.font         = "11px monospace";
      ctx.textAlign    = "center";
      ctx.textBaseline = "middle";
      ctx.fillText(tick.toFixed(0), tx, ty);
    }
  }

  ctx.strokeStyle = "#f8fafc";
  ctx.lineWidth   = 3;
  ctx.lineCap     = "round";
  ctx.beginPath();
  ctx.moveTo(cx, cy);
  ctx.lineTo(cx + Math.cos(valA) * (r - 26), cy + Math.sin(valA) * (r - 26));
  ctx.stroke();
  ctx.lineCap = "butt";

  ctx.fillStyle = arcColor;
  ctx.beginPath();
  ctx.arc(cx, cy, 6, 0, Math.PI * 2);
  ctx.fill();

  ctx.fillStyle    = "#fff";
  ctx.font         = "bold 28px monospace";
  ctx.textAlign    = "center";
  ctx.textBaseline = "alphabetic";
  ctx.fillText(v.toFixed(1), cx, cy + r * 0.55);

  ctx.font      = "12px sans-serif";
  ctx.fillStyle = "#94a3b8";
  ctx.fillText(ds.title + (ds.units ? " (" + ds.units + ")" : ""), cx, cy + r * 0.75);
  ctx.textAlign = "start";
}

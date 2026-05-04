// Progress rings.
//
// Each dataset becomes a circular ring filled in proportion to its value.
// Multiple rings stack visually so users see overall completion at a glance.

function paint(ctx, w, h) {
  ctx.fillStyle = "#0b1220";
  ctx.fillRect(0, 0, w, h);

  if (datasets.length === 0) return;

  const cols = Math.min(datasets.length, 4);
  const rows = Math.ceil(datasets.length / cols);
  const cw   = w / cols;
  const ch   = h / rows;
  const colors = ["#22d3ee", "#a78bfa", "#34d399", "#fbbf24",
                  "#f472b6", "#60a5fa", "#facc15", "#f87171"];

  for (let i = 0; i < datasets.length; ++i) {
    const r  = Math.floor(i / cols);
    const c  = i % cols;
    const cx = cw * c + cw * 0.5;
    const cy = ch * r + ch * 0.5;
    const rr = Math.min(cw, ch) * 0.38;
    const ds = datasets[i];
    const v  = Number.isFinite(ds.value) ? ds.value : 0;
    const norm = Math.max(0, Math.min(1, (v - ds.min) / ((ds.max - ds.min) || 1)));

    ctx.strokeStyle = "#1f2937";
    ctx.lineWidth   = 14;
    ctx.lineCap     = "round";
    ctx.beginPath();
    ctx.arc(cx, cy, rr, 0, Math.PI * 2);
    ctx.stroke();

    ctx.strokeStyle = colors[i % colors.length];
    ctx.beginPath();
    ctx.arc(cx, cy, rr, -Math.PI * 0.5, -Math.PI * 0.5 + Math.PI * 2 * norm);
    ctx.stroke();
    ctx.lineCap = "butt";

    ctx.fillStyle    = "#fff";
    ctx.font         = "bold 18px monospace";
    ctx.textAlign    = "center";
    ctx.textBaseline = "middle";
    ctx.fillText((norm * 100).toFixed(0) + "%", cx, cy - 4);

    ctx.fillStyle = "#94a3b8";
    ctx.font      = "11px sans-serif";
    ctx.fillText(ds.title.substring(0, 14), cx, cy + 16);
  }
  ctx.textAlign = "start";
  ctx.textBaseline = "alphabetic";
}

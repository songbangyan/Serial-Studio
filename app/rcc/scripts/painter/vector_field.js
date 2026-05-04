// 2D vector field.
//
// Treats datasets pairwise as (Vx, Vy). Draws each pair as an arrow whose
// magnitude scales with the vector length and whose direction follows the
// pair angle. Pairs are placed on a regular grid.

function paint(ctx, w, h) {
  ctx.fillStyle = "#0a0a0c";
  ctx.fillRect(0, 0, w, h);

  const pairs = Math.floor(datasets.length / 2);
  if (pairs === 0) return;

  const cols = Math.ceil(Math.sqrt(pairs));
  const rows = Math.ceil(pairs / cols);
  const cw   = w / cols;
  const ch   = h / rows;
  const arrowMax = Math.min(cw, ch) * 0.4;

  for (let i = 0; i < pairs; ++i) {
    const r = Math.floor(i / cols);
    const c = i % cols;
    const cx = cw * (c + 0.5);
    const cy = ch * (r + 0.5);

    const vx = datasets[i * 2].value;
    const vy = datasets[i * 2 + 1].value;
    if (!Number.isFinite(vx) || !Number.isFinite(vy)) continue;

    const xMax = Math.max(Math.abs(datasets[i * 2].min), Math.abs(datasets[i * 2].max), 1);
    const yMax = Math.max(Math.abs(datasets[i * 2 + 1].min), Math.abs(datasets[i * 2 + 1].max), 1);
    const nx   = vx / xMax;
    const ny   = vy / yMax;
    const mag  = Math.min(1, Math.hypot(nx, ny));

    ctx.strokeStyle = "#1f2937";
    ctx.lineWidth   = 1;
    ctx.beginPath();
    ctx.arc(cx, cy, arrowMax, 0, Math.PI * 2);
    ctx.stroke();

    const ex = cx + nx * arrowMax;
    const ey = cy - ny * arrowMax;
    const hue = (Math.atan2(-ny, nx) * 180 / Math.PI + 360) % 360;

    ctx.strokeStyle = `hsl(${hue}, 80%, 60%)`;
    ctx.lineWidth   = 2;
    ctx.lineCap     = "round";
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.lineTo(ex, ey);
    ctx.stroke();

    const a    = Math.atan2(ey - cy, ex - cx);
    const head = 8;
    ctx.fillStyle = `hsl(${hue}, 80%, 60%)`;
    ctx.beginPath();
    ctx.moveTo(ex, ey);
    ctx.lineTo(ex - Math.cos(a - 0.4) * head, ey - Math.sin(a - 0.4) * head);
    ctx.lineTo(ex - Math.cos(a + 0.4) * head, ey - Math.sin(a + 0.4) * head);
    ctx.closePath();
    ctx.fill();
    ctx.lineCap = "butt";

    ctx.fillStyle    = "#94a3b8";
    ctx.font         = "10px monospace";
    ctx.textAlign    = "center";
    ctx.textBaseline = "top";
    ctx.fillText("|v|=" + mag.toFixed(2), cx, cy + arrowMax + 4);
  }
  ctx.textAlign = "start";
  ctx.textBaseline = "alphabetic";
}

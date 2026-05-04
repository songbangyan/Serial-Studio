// 2D vector field.
//
// Treats datasets pairwise as (Vx, Vy). Each pair becomes an arrow on a
// circular dial: the arrow's angle is the vector's direction; its length
// (and colour intensity) scale with the magnitude.

function paint(ctx, w, h) {
  // Cream paper background + vignette.
  ctx.fillStyle = "#f5f5f1";
  ctx.fillRect(0, 0, w, h);
  ctx.strokeStyle = "#e7e5de";
  ctx.lineWidth = 2;
  ctx.strokeRect(1, 1, w - 2, h - 2);

  const pairs = Math.floor(datasets.length / 2);

  // Header.
  ctx.fillStyle = "#0f172a";
  ctx.font = "bold 11px sans-serif";
  ctx.textAlign = "start";
  ctx.fillText("VECTOR  FIELD", 14, 18);
  ctx.fillStyle = "#64748b";
  ctx.font = "10px sans-serif";
  ctx.textAlign = "end";
  ctx.fillText(pairs + (pairs === 1 ? " vector" : " vectors"), w - 14, 18);
  ctx.fillStyle = "#e5e7eb";
  ctx.fillRect(14, 22, w - 28, 1);

  if (pairs === 0) {
    ctx.fillStyle = "#64748b";
    ctx.font = "12px sans-serif";
    ctx.textAlign = "center";
    ctx.fillText("Add datasets in (Vx, Vy) pairs", w / 2, h / 2);
    return;
  }

  // Layout.
  const cols = Math.ceil(Math.sqrt(pairs * w / h));
  const rows = Math.ceil(pairs / cols);
  const padX = 14;
  const padTop = 30;
  const padBot = 14;
  const gap  = 8;
  const cw   = (w - padX * 2 - gap * (cols - 1)) / cols;
  const ch   = (h - padTop - padBot - gap * (rows - 1)) / rows;

  for (let i = 0; i < pairs; ++i) {
    const r  = Math.floor(i / cols);
    const c  = i % cols;
    const x0 = padX + c * (cw + gap);
    const y0 = padTop + r * (ch + gap);
    const cx = x0 + cw * 0.5;
    const cy = y0 + ch * 0.5 + 4;
    const arrowMax = Math.max(8, Math.min(cw, ch - 24) * 0.40);

    // Card.
    ctx.fillStyle = "#e2e8f0";
    ctx.fillRect(x0 + 1, y0 + 2, cw, ch);
    ctx.fillStyle = "#ffffff";
    ctx.fillRect(x0, y0, cw, ch);
    ctx.strokeStyle = "#d4d4d8";
    ctx.lineWidth = 1;
    ctx.strokeRect(x0 + 0.5, y0 + 0.5, cw - 1, ch - 1);

    // Mini header.
    const dsX = datasets[i * 2];
    const dsY = datasets[i * 2 + 1];
    ctx.fillStyle = "#475569";
    ctx.font = "bold 10px sans-serif";
    ctx.textAlign = "start";
    ctx.fillText((dsX.title || "Vx") + ", " + (dsY.title || "Vy"),
                 x0 + 8, y0 + 14);

    // Concentric reference rings (3 levels).
    ctx.strokeStyle = "#eef2f7";
    for (let k = 1; k <= 3; ++k) {
      const rr = arrowMax * (k / 3);
      ctx.beginPath();
      ctx.moveTo(cx + rr, cy);
      ctx.arc(cx, cy, rr, 0, Math.PI * 2);
      ctx.stroke();
    }

    // Cross-hair.
    ctx.strokeStyle = "#cbd5e1";
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(cx - arrowMax, cy);
    ctx.lineTo(cx + arrowMax, cy);
    ctx.moveTo(cx, cy - arrowMax);
    ctx.lineTo(cx, cy + arrowMax);
    ctx.stroke();

    // Outer ring.
    ctx.strokeStyle = "#94a3b8";
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(cx + arrowMax, cy);
    ctx.arc(cx, cy, arrowMax, 0, Math.PI * 2);
    ctx.stroke();

    const vx = dsX.value;
    const vy = dsY.value;
    if (!Number.isFinite(vx) || !Number.isFinite(vy)) continue;

    const xMax = Math.max(Math.abs(dsX.min), Math.abs(dsX.max), 1);
    const yMax = Math.max(Math.abs(dsY.min), Math.abs(dsY.max), 1);
    const nx   = vx / xMax;
    const ny   = vy / yMax;
    const mag  = Math.min(1, Math.hypot(nx, ny));
    const ex = cx + nx * arrowMax;
    const ey = cy - ny * arrowMax;

    // Arrow colour by magnitude (slate -> emerald -> amber -> crimson).
    let color = "#94a3b8";
    if (mag > 0.85) color = "#dc2626";
    else if (mag > 0.55) color = "#f59e0b";
    else if (mag > 0.10) color = "#10b981";

    ctx.strokeStyle = color;
    ctx.lineWidth   = 2.5;
    ctx.lineCap     = "round";
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.lineTo(ex, ey);
    ctx.stroke();

    // Arrowhead.
    const a = Math.atan2(ey - cy, ex - cx);
    const head = 9;
    ctx.fillStyle = color;
    ctx.beginPath();
    ctx.moveTo(ex, ey);
    ctx.lineTo(ex - Math.cos(a - 0.4) * head, ey - Math.sin(a - 0.4) * head);
    ctx.lineTo(ex - Math.cos(a + 0.4) * head, ey - Math.sin(a + 0.4) * head);
    ctx.closePath();
    ctx.fill();
    ctx.lineCap = "butt";

    // Pivot dot.
    ctx.fillStyle = "#0f172a";
    ctx.beginPath();
    ctx.moveTo(cx + 3, cy);
    ctx.arc(cx, cy, 3, 0, Math.PI * 2);
    ctx.fill();

    // Magnitude readout.
    ctx.fillStyle = "#0f172a";
    ctx.font = "bold 11px sans-serif";
    ctx.textAlign = "center";
    ctx.textBaseline = "alphabetic";
    ctx.fillText("|v| = " + mag.toFixed(2), cx, y0 + ch - 6);
  }

  ctx.textAlign = "start";
  ctx.textBaseline = "alphabetic";
}

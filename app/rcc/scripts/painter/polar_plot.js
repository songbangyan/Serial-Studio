// Polar plot.
//
// Treats datasets pairwise as (angle_deg, magnitude). Useful for radar-style
// or compass-rose visualisations. Each pair produces one ray.

function paint(ctx, w, h) {
  ctx.fillStyle = "#0a0a0c";
  ctx.fillRect(0, 0, w, h);

  const cx = w * 0.5;
  const cy = h * 0.5;
  const r  = Math.min(w, h) * 0.42;

  // Concentric rings
  ctx.strokeStyle = "#26313d";
  ctx.lineWidth   = 1;
  for (let i = 1; i <= 4; ++i) {
    ctx.beginPath();
    ctx.arc(cx, cy, (r / 4) * i, 0, Math.PI * 2);
    ctx.stroke();
  }

  // Cardinal cross
  ctx.beginPath();
  ctx.moveTo(cx - r, cy);
  ctx.lineTo(cx + r, cy);
  ctx.moveTo(cx, cy - r);
  ctx.lineTo(cx, cy + r);
  ctx.stroke();

  // Pairs
  ctx.strokeStyle = "#22d3ee";
  ctx.lineWidth   = 2;
  for (let i = 0; i + 1 < datasets.length; i += 2) {
    const angDeg = datasets[i].value;
    const mag    = datasets[i + 1].value;
    if (!Number.isFinite(angDeg) || !Number.isFinite(mag))
      continue;

    const max  = datasets[i + 1].max || 1;
    const norm = Math.max(0, Math.min(1, mag / max));
    const ang  = (angDeg - 90) * Math.PI / 180;
    const x    = cx + Math.cos(ang) * r * norm;
    const y    = cy + Math.sin(ang) * r * norm;

    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.lineTo(x, y);
    ctx.stroke();

    ctx.fillStyle = "#22d3ee";
    ctx.beginPath();
    ctx.arc(x, y, 3, 0, Math.PI * 2);
    ctx.fill();
  }
}

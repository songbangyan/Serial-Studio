// Artificial horizon (attitude indicator).
//
// Reads pitch (deg) from datasets[0] and roll (deg) from datasets[1].
// Falls back to zero if missing. Demonstrates rotate / translate / clip
// composition.

function paint(ctx, w, h) {
  const cx     = w * 0.5;
  const cy     = h * 0.5;
  const r      = Math.min(w, h) * 0.45;
  const pitch  = datasets.length > 0 ? (datasets[0].value || 0) : 0;
  const roll   = datasets.length > 1 ? (datasets[1].value || 0) : 0;
  const pxPerD = r / 30;

  ctx.fillStyle = "#0a0a0c";
  ctx.fillRect(0, 0, w, h);

  ctx.save();
  ctx.beginPath();
  // moveTo before arc so the chord from origin to the arc start isn't
  // included in the clip region.
  ctx.moveTo(cx + r, cy);
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.clip();

  ctx.translate(cx, cy);
  ctx.rotate(-roll * Math.PI / 180);
  ctx.translate(0, pitch * pxPerD);

  ctx.fillStyle = "#3b82f6";
  ctx.fillRect(-r * 2, -r * 2, r * 4, r * 2);
  ctx.fillStyle = "#7c3f00";
  ctx.fillRect(-r * 2, 0, r * 4, r * 2);

  ctx.strokeStyle = "#fff";
  ctx.lineWidth   = 2;
  ctx.beginPath();
  ctx.moveTo(-r, 0);
  ctx.lineTo(r, 0);
  ctx.stroke();

  ctx.lineWidth = 1;
  for (let p = -90; p <= 90; p += 10) {
    if (p === 0) continue;
    const y    = -p * pxPerD;
    const half = (p % 30 === 0) ? r * 0.25 : r * 0.12;
    ctx.beginPath();
    ctx.moveTo(-half, y);
    ctx.lineTo(half, y);
    ctx.stroke();

    if (p % 30 === 0) {
      ctx.fillStyle    = "#fff";
      ctx.font         = "10px monospace";
      ctx.textAlign    = "right";
      ctx.textBaseline = "middle";
      ctx.fillText(String(Math.abs(p)), -half - 4, y);
      ctx.textAlign    = "left";
      ctx.fillText(String(Math.abs(p)), half + 4, y);
    }
  }

  ctx.restore();

  ctx.strokeStyle = "#fde047";
  ctx.lineWidth   = 3;
  ctx.beginPath();
  ctx.moveTo(cx - r * 0.3, cy);
  ctx.lineTo(cx - r * 0.05, cy);
  ctx.moveTo(cx + r * 0.05, cy);
  ctx.lineTo(cx + r * 0.3, cy);
  ctx.moveTo(cx, cy - r * 0.05);
  ctx.lineTo(cx, cy + r * 0.05);
  ctx.stroke();

  ctx.strokeStyle = "#888";
  ctx.lineWidth   = 1.5;
  ctx.beginPath();
  ctx.moveTo(cx + r, cy);
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.stroke();

  ctx.fillStyle    = "#cbd5e1";
  ctx.font         = "11px monospace";
  ctx.textAlign    = "left";
  ctx.textBaseline = "alphabetic";
  ctx.fillText("PITCH " + pitch.toFixed(1) + "°", 8, 16);
  ctx.fillText("ROLL  " + roll.toFixed(1) + "°", 8, 30);
  ctx.textAlign = "start";
}

// Artificial horizon (attitude indicator).
//
// Reads pitch (deg) from datasets[0] and roll (deg) from datasets[1].
// Falls back to zero if missing. Demonstrates rotate / translate / clip
// composition. Kept dark on purpose -- a light artificial horizon would
// stop reading as one. The cream-paper card frame around the dial keeps
// it consistent with the rest of the template family.

function paint(ctx, w, h) {
  const pitch  = datasets.length > 0 ? (datasets[0].value || 0) : 0;
  const roll   = datasets.length > 1 ? (datasets[1].value || 0) : 0;

  // Cream paper background + vignette.
  ctx.fillStyle = "#f5f5f1";
  ctx.fillRect(0, 0, w, h);
  ctx.strokeStyle = "#e7e5de";
  ctx.lineWidth = 2;
  ctx.strokeRect(1, 1, w - 2, h - 2);

  // Header strip.
  ctx.fillStyle = "#0f172a";
  ctx.font = "bold 11px sans-serif";
  ctx.textAlign = "start";
  ctx.fillText("ATTITUDE  INDICATOR", 14, 18);
  ctx.fillStyle = "#64748b";
  ctx.font = "10px sans-serif";
  ctx.textAlign = "end";
  ctx.fillText("pitch / roll", w - 14, 18);
  ctx.fillStyle = "#e5e7eb";
  ctx.fillRect(14, 22, w - 28, 1);

  // Card containing the dark instrument.
  const padX = 14;
  const padTop = 30;
  const padBot = 36;
  const cardX = padX;
  const cardY = padTop;
  const cardW = w - padX * 2;
  const cardH = h - padTop - padBot;

  ctx.fillStyle = "#e2e8f0";
  ctx.fillRect(cardX + 1, cardY + 2, cardW, cardH);
  ctx.fillStyle = "#0a0a0c";
  ctx.fillRect(cardX, cardY, cardW, cardH);

  const cx = cardX + cardW * 0.5;
  const cy = cardY + cardH * 0.5;
  const r  = Math.min(cardW, cardH) * 0.42;
  const pxPerD = r / 30;

  // Clip to dial circle. moveTo before arc so the chord from the implicit
  // origin doesn't carve a wedge out of the clip region.
  ctx.save();
  ctx.beginPath();
  ctx.moveTo(cx + r, cy);
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.clip();

  ctx.translate(cx, cy);
  ctx.rotate(-roll * Math.PI / 180);
  ctx.translate(0, pitch * pxPerD);

  // Sky / ground.
  ctx.fillStyle = "#3b82f6";
  ctx.fillRect(-r * 2, -r * 2, r * 4, r * 2);
  ctx.fillStyle = "#7c3f00";
  ctx.fillRect(-r * 2, 0, r * 4, r * 2);

  // Horizon line.
  ctx.strokeStyle = "#ffffff";
  ctx.lineWidth   = 2;
  ctx.beginPath();
  ctx.moveTo(-r, 0);
  ctx.lineTo(r, 0);
  ctx.stroke();

  // Pitch ladder.
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
      ctx.fillStyle    = "#ffffff";
      ctx.font         = "10px monospace";
      ctx.textAlign    = "right";
      ctx.textBaseline = "middle";
      ctx.fillText(String(Math.abs(p)), -half - 4, y);
      ctx.textAlign    = "left";
      ctx.fillText(String(Math.abs(p)), half + 4, y);
    }
  }

  ctx.restore();

  // Aircraft symbol.
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

  // Instrument bezel.
  ctx.strokeStyle = "#475569";
  ctx.lineWidth   = 2;
  ctx.beginPath();
  ctx.moveTo(cx + r, cy);
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.stroke();

  // Card border.
  ctx.strokeStyle = "#1f2937";
  ctx.lineWidth = 1;
  ctx.strokeRect(cardX + 0.5, cardY + 0.5, cardW - 1, cardH - 1);

  // Pitch + roll readouts at the bottom on the cream area.
  ctx.fillStyle    = "#0f172a";
  ctx.font         = "bold 12px sans-serif";
  ctx.textAlign    = "start";
  ctx.textBaseline = "alphabetic";
  ctx.fillText("PITCH  " + pitch.toFixed(1) + "°", padX + 6, h - 12);
  ctx.textAlign    = "end";
  ctx.fillText("ROLL  " + roll.toFixed(1) + "°", w - padX - 6, h - 12);

  ctx.textAlign = "start";
}

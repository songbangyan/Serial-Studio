// Vertical bars with peak-hold markers.
//
// Renders one bar per dataset; the peak-hold line decays linearly so the
// most recent maximum is briefly visible after a spike.

const peaks = [];
const decayPerSec = 0.5;
let lastTs = 0;

function onFrame() {
  const now = frame.timestampMs * 1e-3;
  const dt  = lastTs > 0 ? Math.max(0, now - lastTs) : 0;
  lastTs    = now;

  for (let i = 0; i < datasets.length; ++i) {
    const v = datasets[i].value;
    if (!Number.isFinite(v))
      continue;

    const max  = datasets[i].max || 1;
    const norm = Math.max(0, Math.min(1, v / max));
    if (peaks[i] === undefined)
      peaks[i] = norm;

    if (norm >= peaks[i])
      peaks[i] = norm;
    else
      peaks[i] = Math.max(0, peaks[i] - decayPerSec * dt);
  }
}

function paint(ctx, w, h) {
  ctx.fillStyle = "#0d1117";
  ctx.fillRect(0, 0, w, h);

  if (datasets.length === 0)
    return;

  const padX = 16;
  const padY = 16;
  const labelH = 14;
  const cellW  = (w - padX * 2) / datasets.length;
  const barW   = cellW * 0.6;
  const top    = padY;
  const bottom = h - padY - labelH;

  for (let i = 0; i < datasets.length; ++i) {
    const x  = padX + i * cellW + (cellW - barW) * 0.5;
    const v  = datasets[i].value;
    const mx = datasets[i].max || 1;
    const n  = Math.max(0, Math.min(1, v / mx));
    const bh = (bottom - top) * n;
    const by = bottom - bh;

    ctx.fillStyle = "#1f2937";
    ctx.fillRect(x, top, barW, bottom - top);

    ctx.fillStyle = "#22c55e";
    ctx.fillRect(x, by, barW, bh);

    if (peaks[i] !== undefined) {
      const py = bottom - (bottom - top) * peaks[i];
      ctx.strokeStyle = "#fde047";
      ctx.lineWidth   = 2;
      ctx.beginPath();
      ctx.moveTo(x - 2, py);
      ctx.lineTo(x + barW + 2, py);
      ctx.stroke();
    }

    ctx.fillStyle    = "#cbd5e1";
    ctx.font         = "11px sans-serif";
    ctx.textAlign    = "center";
    ctx.fillText(datasets[i].title, x + barW * 0.5, h - padY * 0.5);
    ctx.textAlign    = "start";
  }
}

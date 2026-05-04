// Audio-style VU meter with peak hold.
//
// Each dataset becomes a vertical bar with a green/yellow/red gradient and a
// decaying peak marker. Useful for level meters, signal strength, RMS values.

const PEAK_HOLD_S = 1.2;
const peaks = [];
const peakTs = [];
let lastTs = 0;

function onFrame() {
  const now = frame.timestampMs * 1e-3;
  const dt  = lastTs > 0 ? Math.max(0, now - lastTs) : 0;
  lastTs    = now;

  while (peaks.length < datasets.length) {
    peaks.push(0);
    peakTs.push(0);
  }

  for (let i = 0; i < datasets.length; ++i) {
    const ds   = datasets[i];
    const norm = Number.isFinite(ds.value)
      ? Math.max(0, Math.min(1, (ds.value - ds.min) / ((ds.max - ds.min) || 1)))
      : 0;

    if (norm >= peaks[i] || (now - peakTs[i]) > PEAK_HOLD_S) {
      peaks[i] = norm;
      peakTs[i] = now;
    } else {
      peaks[i] = Math.max(0, peaks[i] - dt * 0.4);
    }
  }
}

function paint(ctx, w, h) {
  ctx.fillStyle = "#0a0a0c";
  ctx.fillRect(0, 0, w, h);

  if (datasets.length === 0) return;

  const padX = 12;
  const padT = 12;
  const padB = 36;
  const cellW = (w - padX * 2) / datasets.length;
  const barW  = cellW * 0.55;
  const top   = padT;
  const bot   = h - padB;

  for (let i = 0; i < datasets.length; ++i) {
    const ds   = datasets[i];
    const x    = padX + i * cellW + (cellW - barW) * 0.5;
    const norm = Number.isFinite(ds.value)
      ? Math.max(0, Math.min(1, (ds.value - ds.min) / ((ds.max - ds.min) || 1)))
      : 0;
    const filled = (bot - top) * norm;

    ctx.fillStyle = "#111827";
    ctx.fillRect(x, top, barW, bot - top);

    // Stack of color bands: green (low), yellow (mid), red (top)
    const greenH = (bot - top) * 0.66 * Math.min(norm / 0.66, 1);
    const yellH  = norm > 0.66 ? (bot - top) * 0.19 * Math.min((norm - 0.66) / 0.19, 1) : 0;
    const redH   = norm > 0.85 ? (bot - top) * 0.15 * Math.min((norm - 0.85) / 0.15, 1) : 0;

    let yCursor = bot;
    ctx.fillStyle = "#22c55e";
    ctx.fillRect(x, yCursor - greenH, barW, greenH);
    yCursor -= greenH;
    ctx.fillStyle = "#facc15";
    ctx.fillRect(x, yCursor - yellH, barW, yellH);
    yCursor -= yellH;
    ctx.fillStyle = "#ef4444";
    ctx.fillRect(x, yCursor - redH, barW, redH);

    // Peak marker
    const peakY = bot - (bot - top) * peaks[i];
    ctx.fillStyle = "#f8fafc";
    ctx.fillRect(x - 1, peakY - 1, barW + 2, 2);

    // Tick marks (-3 dB style: every 10%)
    ctx.strokeStyle = "#374151";
    ctx.lineWidth   = 1;
    for (let k = 0; k <= 10; ++k) {
      const ty = bot - ((bot - top) / 10) * k;
      ctx.beginPath();
      ctx.moveTo(x + barW + 2, ty);
      ctx.lineTo(x + barW + 6, ty);
      ctx.stroke();
    }

    ctx.fillStyle    = "#cbd5e1";
    ctx.font         = "10px sans-serif";
    ctx.textAlign    = "center";
    ctx.fillText(ds.title, x + barW * 0.5, h - 18);
    ctx.fillStyle    = "#fff";
    ctx.font         = "bold 11px monospace";
    ctx.fillText(Number.isFinite(ds.value) ? ds.value.toFixed(1) : "—",
                 x + barW * 0.5, h - 4);
  }
  ctx.textAlign = "start";
}

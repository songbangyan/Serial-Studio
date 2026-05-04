// Radar sweep with afterglow.
//
// Pairs of datasets are treated as (azimuth_deg, range). The sweep line
// rotates at a fixed rate; recent contacts fade out behind it. Kept dark
// on purpose -- the back-lit PPI look is what makes a radar a radar. The
// cream paper card frame ties it back to the rest of the templates.

const HISTORY_MS = 4000;
const SWEEP_RATE_DEG = 60;
let lastTs = 0;
let sweepDeg = 0;
let contacts = [];

function onFrame() {
  const now = frame.timestampMs;
  const dt  = lastTs > 0 ? Math.max(0, now - lastTs) : 16;
  lastTs    = now;

  sweepDeg = (sweepDeg + SWEEP_RATE_DEG * dt * 1e-3) % 360;

  for (let i = 0; i + 1 < datasets.length; i += 2) {
    const az  = datasets[i].value;
    const rng = datasets[i + 1].value;
    if (!Number.isFinite(az) || !Number.isFinite(rng)) continue;
    contacts.push({ az: az, rng: rng, max: datasets[i + 1].max || 1, ts: now });
  }
  contacts = contacts.filter(c => now - c.ts < HISTORY_MS);
}

function paint(ctx, w, h) {
  // Cream paper background + vignette.
  ctx.fillStyle = "#f5f5f1";
  ctx.fillRect(0, 0, w, h);
  ctx.strokeStyle = "#e7e5de";
  ctx.lineWidth = 2;
  ctx.strokeRect(1, 1, w - 2, h - 2);

  // Header.
  const pairs = Math.floor(datasets.length / 2);
  ctx.fillStyle = "#0f172a";
  ctx.font = "bold 11px sans-serif";
  ctx.textAlign = "start";
  ctx.fillText("RADAR  PPI", 14, 18);
  ctx.fillStyle = "#64748b";
  ctx.font = "10px sans-serif";
  ctx.textAlign = "end";
  ctx.fillText(contacts.length + " contacts", w - 14, 18);
  ctx.fillStyle = "#e5e7eb";
  ctx.fillRect(14, 22, w - 28, 1);

  // Card containing the dark scope.
  const padX = 14;
  const padTop = 30;
  const padBot = 14;
  const cardX = padX;
  const cardY = padTop;
  const cardW = w - padX * 2;
  const cardH = h - padTop - padBot;

  ctx.fillStyle = "#e2e8f0";
  ctx.fillRect(cardX + 1, cardY + 2, cardW, cardH);
  ctx.fillStyle = "#03110a";
  ctx.fillRect(cardX, cardY, cardW, cardH);

  const cx = cardX + cardW * 0.5;
  const cy = cardY + cardH * 0.5;
  const r  = Math.min(cardW, cardH) * 0.42;

  // Range rings.
  ctx.strokeStyle = "#0f6b3a";
  ctx.lineWidth   = 1;
  for (let i = 1; i <= 4; ++i) {
    const rr = (r / 4) * i;
    ctx.beginPath();
    ctx.moveTo(cx + rr, cy);
    ctx.arc(cx, cy, rr, 0, Math.PI * 2);
    ctx.stroke();
  }

  // Bearing spokes every 30 degrees.
  for (let a = 0; a < 360; a += 30) {
    const rad = (a - 90) * Math.PI / 180;
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.lineTo(cx + Math.cos(rad) * r, cy + Math.sin(rad) * r);
    ctx.stroke();
  }

  // Cardinal labels.
  ctx.fillStyle    = "#86efac";
  ctx.font         = "bold 9px monospace";
  ctx.textAlign    = "center";
  ctx.textBaseline = "middle";
  ctx.fillText("N", cx,           cy - r - 8);
  ctx.fillText("S", cx,           cy + r + 8);
  ctx.fillText("E", cx + r + 8,   cy);
  ctx.fillText("W", cx - r - 8,   cy);
  ctx.textBaseline = "alphabetic";

  // Sweep wedge: 12 fading slivers.
  const sweepRad = (sweepDeg - 90) * Math.PI / 180;
  for (let i = 0; i < 12; ++i) {
    const a0 = sweepRad - (i + 1) * 0.04;
    const a1 = sweepRad - i * 0.04;
    ctx.globalAlpha = (1 - i / 12) * 0.6;
    ctx.fillStyle   = "#22c55e";
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.arc(cx, cy, r, a0, a1);
    ctx.closePath();
    ctx.fill();
  }
  ctx.globalAlpha = 1;

  // Outer scope ring.
  ctx.strokeStyle = "#155e30";
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(cx + r, cy);
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.stroke();

  // Contacts (older = dimmer).
  for (const c of contacts) {
    const age   = (frame.timestampMs - c.ts) / HISTORY_MS;
    const rad   = (c.az - 90) * Math.PI / 180;
    const norm  = Math.max(0, Math.min(1, c.rng / c.max));
    const x     = cx + Math.cos(rad) * r * norm;
    const y     = cy + Math.sin(rad) * r * norm;
    ctx.globalAlpha = 1 - age;
    ctx.fillStyle   = "#bbf7d0";
    ctx.beginPath();
    ctx.moveTo(x + 4, y);
    ctx.arc(x, y, 4, 0, Math.PI * 2);
    ctx.fill();
  }
  ctx.globalAlpha = 1;

  // Card border on top of the dark area.
  ctx.strokeStyle = "#1f2937";
  ctx.lineWidth = 1;
  ctx.strokeRect(cardX + 0.5, cardY + 0.5, cardW - 1, cardH - 1);
}

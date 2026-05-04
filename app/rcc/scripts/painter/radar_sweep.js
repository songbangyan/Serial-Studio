// Radar sweep with afterglow.
//
// Pairs of datasets are treated as (azimuth_deg, range). The sweep line
// rotates at a fixed rate; recent contacts fade out behind it.

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

  // Stamp the most recent (azimuth, range) pair as a contact
  for (let i = 0; i + 1 < datasets.length; i += 2) {
    const az  = datasets[i].value;
    const rng = datasets[i + 1].value;
    if (!Number.isFinite(az) || !Number.isFinite(rng))
      continue;

    contacts.push({ az: az, rng: rng, max: datasets[i + 1].max || 1, ts: now });
  }

  contacts = contacts.filter(c => now - c.ts < HISTORY_MS);
}

function paint(ctx, w, h) {
  ctx.fillStyle = "#03110a";
  ctx.fillRect(0, 0, w, h);

  const cx = w * 0.5;
  const cy = h * 0.5;
  const r  = Math.min(w, h) * 0.45;

  ctx.strokeStyle = "#0f6b3a";
  ctx.lineWidth   = 1;
  for (let i = 1; i <= 4; ++i) {
    const rr = (r / 4) * i;
    ctx.beginPath();
    ctx.moveTo(cx + rr, cy);
    ctx.arc(cx, cy, rr, 0, Math.PI * 2);
    ctx.stroke();
  }

  for (let a = 0; a < 360; a += 30) {
    const rad = (a - 90) * Math.PI / 180;
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.lineTo(cx + Math.cos(rad) * r, cy + Math.sin(rad) * r);
    ctx.stroke();
  }

  // Sweep gradient via 12 wedge segments with fading alpha
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
}

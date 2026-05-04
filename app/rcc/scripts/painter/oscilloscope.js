// Oscilloscope.
//
// Per-channel trace with phosphor-style afterglow on a CRT background.
// Channels are spread vertically so traces don't overlap.

const HISTORY = 256;
const traces  = [];

function onFrame() {
  while (traces.length < datasets.length) traces.push([]);

  for (let i = 0; i < datasets.length; ++i) {
    const v = datasets[i].value;
    if (Number.isFinite(v)) {
      traces[i].push(v);
      if (traces[i].length > HISTORY) traces[i].shift();
    }
  }
}

function paint(ctx, w, h) {
  ctx.fillStyle = "#06140a";
  ctx.fillRect(0, 0, w, h);

  ctx.strokeStyle = "#0c3a1c";
  ctx.lineWidth   = 1;
  for (let i = 1; i < 10; ++i) {
    const x = (w / 10) * i;
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, h);
    ctx.stroke();
  }
  for (let i = 1; i < 8; ++i) {
    const y = (h / 8) * i;
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(w, y);
    ctx.stroke();
  }

  if (datasets.length === 0) return;

  const lanes = datasets.length;
  const laneH = h / lanes;

  for (let i = 0; i < traces.length; ++i) {
    const t  = traces[i];
    if (t.length < 2) continue;
    const ds = datasets[i];
    const lo = ds.min;
    const hi = ds.max;
    const cy = laneH * (i + 0.5);

    ctx.strokeStyle = "#22c55e";
    ctx.lineWidth   = 2;
    ctx.beginPath();
    for (let k = 0; k < t.length; ++k) {
      const x = (k / (HISTORY - 1)) * w;
      const n = (t[k] - lo) / ((hi - lo) || 1);
      const y = cy - (n - 0.5) * laneH * 0.85;
      k === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
    }
    ctx.stroke();

    ctx.fillStyle    = "#86efac";
    ctx.font         = "10px monospace";
    ctx.textBaseline = "top";
    ctx.fillText("CH" + (i + 1) + " " + ds.title, 6, i * laneH + 4);
  }
  ctx.textBaseline = "alphabetic";
}

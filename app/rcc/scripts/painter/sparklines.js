// Sparkline grid.
//
// One miniature line chart per dataset, laid out in a grid. Each cell shows
// title, current value, and a tiny history trace. Useful overview when many
// channels are being tracked.

const HISTORY = 60;
const traces  = [];

function onFrame() {
  while (traces.length < datasets.length)
    traces.push([]);

  for (let i = 0; i < datasets.length; ++i) {
    const v = datasets[i].value;
    if (Number.isFinite(v)) {
      traces[i].push(v);
      if (traces[i].length > HISTORY)
        traces[i].shift();
    }
  }
}

function paint(ctx, w, h) {
  ctx.fillStyle = "#0b1220";
  ctx.fillRect(0, 0, w, h);

  if (datasets.length === 0) return;

  const cols = Math.max(1, Math.ceil(Math.sqrt(datasets.length)));
  const rows = Math.ceil(datasets.length / cols);
  const gap  = 6;
  const cw   = (w - gap * (cols + 1)) / cols;
  const ch   = (h - gap * (rows + 1)) / rows;

  for (let i = 0; i < datasets.length; ++i) {
    const r = Math.floor(i / cols);
    const c = i % cols;
    const x = gap + c * (cw + gap);
    const y = gap + r * (ch + gap);
    const ds = datasets[i];

    ctx.fillStyle = "#111827";
    ctx.fillRect(x, y, cw, ch);

    ctx.fillStyle    = "#cbd5e1";
    ctx.font         = "11px sans-serif";
    ctx.textBaseline = "top";
    ctx.fillText(ds.title.substring(0, 18), x + 8, y + 6);

    ctx.fillStyle = "#fff";
    ctx.font      = "bold 18px monospace";
    ctx.textBaseline = "alphabetic";
    const valTxt = (Number.isFinite(ds.value) ? ds.value.toFixed(1) : "—") +
                   (ds.units ? " " + ds.units : "");
    ctx.fillText(valTxt, x + 8, y + 36);

    const t = traces[i];
    if (t && t.length >= 2) {
      const lo = ds.min;
      const hi = ds.max;
      const px = x + 4;
      const py = y + ch - 4;
      const pw = cw - 8;
      const ph = ch * 0.4;

      ctx.strokeStyle = "#22d3ee";
      ctx.lineWidth   = 1.4;
      ctx.beginPath();
      for (let k = 0; k < t.length; ++k) {
        const xk = px + (k / (HISTORY - 1)) * pw;
        const n  = (t[k] - lo) / ((hi - lo) || 1);
        const yk = py - Math.max(0, Math.min(1, n)) * ph;
        k === 0 ? ctx.moveTo(xk, yk) : ctx.lineTo(xk, yk);
      }
      ctx.stroke();
    }
  }
  ctx.textBaseline = "alphabetic";
}

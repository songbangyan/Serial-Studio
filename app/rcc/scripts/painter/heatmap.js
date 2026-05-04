// Heatmap.
//
// Treats datasets[0..N-1] as a row vector pushed onto a scrolling 2D image.
// Shows a colour-mapped time-vs-channel heatmap, useful for spectrograms,
// thermal arrays, or multi-channel snapshots over time.

const HISTORY = 120;
const rows = [];

function onFrame() {
  const n = datasets.length;
  if (n === 0) return;

  const row = new Array(n);
  let lo = Number.POSITIVE_INFINITY, hi = Number.NEGATIVE_INFINITY;
  for (let i = 0; i < n; ++i) {
    lo = Math.min(lo, datasets[i].min);
    hi = Math.max(hi, datasets[i].max);
  }
  if (!Number.isFinite(lo) || !Number.isFinite(hi) || hi <= lo) {
    lo = 0; hi = 1;
  }

  for (let i = 0; i < n; ++i) {
    const v = datasets[i].value;
    row[i] = Number.isFinite(v) ? Math.max(0, Math.min(1, (v - lo) / (hi - lo))) : 0;
  }

  rows.push(row);
  if (rows.length > HISTORY) rows.shift();
}

function viridis(t) {
  // 5-stop approximation of viridis
  const stops = [
    [0.000, 68,   1,  84],
    [0.250, 59,  82, 139],
    [0.500, 33, 145, 140],
    [0.750, 94, 201,  98],
    [1.000, 253, 231, 37]
  ];
  for (let i = 1; i < stops.length; ++i) {
    if (t <= stops[i][0]) {
      const a = stops[i - 1], b = stops[i];
      const k = (t - a[0]) / (b[0] - a[0]);
      const r = a[1] + (b[1] - a[1]) * k;
      const g = a[2] + (b[2] - a[2]) * k;
      const bl = a[3] + (b[3] - a[3]) * k;
      return `rgb(${r | 0}, ${g | 0}, ${bl | 0})`;
    }
  }
  return "rgb(253, 231, 37)";
}

function paint(ctx, w, h) {
  ctx.fillStyle = "#000";
  ctx.fillRect(0, 0, w, h);

  if (rows.length === 0 || datasets.length === 0) return;

  const cols = datasets.length;
  const cellW = w / HISTORY;
  const cellH = h / cols;

  for (let r = 0; r < rows.length; ++r) {
    const row = rows[r];
    const x   = r * cellW;
    for (let c = 0; c < cols; ++c) {
      ctx.fillStyle = viridis(row[c]);
      ctx.fillRect(x, c * cellH, cellW + 1, cellH + 1);
    }
  }
}

// XY scope (Lissajous mode).
//
// Treats datasets pairwise as (X, Y). For each pair, draws the latest
// trajectory in XY space (think analog oscilloscope in X-Y mode).
// Useful for phase relationships, gait cycles, attractor visualisations.

const HISTORY = 200;
const trails  = [];

function onFrame() {
  const pairs = Math.floor(datasets.length / 2);
  while (trails.length < pairs) trails.push([]);

  for (let p = 0; p < pairs; ++p) {
    const x = datasets[p * 2].value;
    const y = datasets[p * 2 + 1].value;
    if (!Number.isFinite(x) || !Number.isFinite(y)) continue;

    trails[p].push([x, y]);
    if (trails[p].length > HISTORY) trails[p].shift();
  }
}

function paint(ctx, w, h) {
  ctx.fillStyle = "#06070a";
  ctx.fillRect(0, 0, w, h);

  const pairs = trails.length;
  if (pairs === 0) return;

  const cols = Math.ceil(Math.sqrt(pairs));
  const rows = Math.ceil(pairs / cols);
  const cw   = w / cols;
  const ch   = h / rows;

  const colors = ["#22d3ee", "#a78bfa", "#fbbf24", "#f472b6"];

  for (let p = 0; p < pairs; ++p) {
    const r  = Math.floor(p / cols);
    const c  = p % cols;
    const x0 = c * cw;
    const y0 = r * ch;
    const cx = x0 + cw * 0.5;
    const cy = y0 + ch * 0.5;
    const sz = Math.min(cw, ch) * 0.4;

    ctx.strokeStyle = "#1f2937";
    ctx.lineWidth   = 1;
    ctx.strokeRect(x0 + 4, y0 + 4, cw - 8, ch - 8);
    ctx.beginPath();
    ctx.moveTo(x0 + 4, cy); ctx.lineTo(x0 + cw - 4, cy);
    ctx.moveTo(cx, y0 + 4); ctx.lineTo(cx, y0 + ch - 4);
    ctx.stroke();

    const t   = trails[p];
    if (t.length < 2) continue;
    const dsX = datasets[p * 2];
    const dsY = datasets[p * 2 + 1];
    const xLo = dsX.min, xHi = dsX.max;
    const yLo = dsY.min, yHi = dsY.max;

    ctx.strokeStyle = colors[p % colors.length];
    ctx.lineWidth   = 1.5;
    for (let k = 1; k < t.length; ++k) {
      const a = t[k - 1], b = t[k];
      const xa = cx + ((a[0] - (xLo + xHi) * 0.5) / ((xHi - xLo) || 1)) * sz * 2;
      const ya = cy - ((a[1] - (yLo + yHi) * 0.5) / ((yHi - yLo) || 1)) * sz * 2;
      const xb = cx + ((b[0] - (xLo + xHi) * 0.5) / ((xHi - xLo) || 1)) * sz * 2;
      const yb = cy - ((b[1] - (yLo + yHi) * 0.5) / ((yHi - yLo) || 1)) * sz * 2;

      ctx.globalAlpha = k / t.length;
      ctx.beginPath();
      ctx.moveTo(xa, ya);
      ctx.lineTo(xb, yb);
      ctx.stroke();
    }
    ctx.globalAlpha = 1;

    // Latest position dot
    const last = t[t.length - 1];
    const lx = cx + ((last[0] - (xLo + xHi) * 0.5) / ((xHi - xLo) || 1)) * sz * 2;
    const ly = cy - ((last[1] - (yLo + yHi) * 0.5) / ((yHi - yLo) || 1)) * sz * 2;
    ctx.fillStyle = colors[p % colors.length];
    ctx.beginPath();
    ctx.moveTo(lx + 3, ly);
    ctx.arc(lx, ly, 3, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle    = "#94a3b8";
    ctx.font         = "10px monospace";
    ctx.textBaseline = "top";
    ctx.fillText(dsX.title + " × " + dsY.title, x0 + 8, y0 + 8);
  }
  ctx.textBaseline = "alphabetic";
}

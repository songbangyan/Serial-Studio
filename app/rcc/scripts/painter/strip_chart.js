// Scrolling strip chart.
//
// Each dataset gets its own colored line trace. History grows until the
// configured length, then scrolls. Demonstrates ring-buffer state +
// per-trace coloring.

const HISTORY = 240;
const COLORS  = ["#22d3ee", "#a78bfa", "#fbbf24", "#f472b6", "#34d399",
                 "#f87171", "#60a5fa", "#facc15"];

const traces = [];

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
  ctx.fillStyle = "#0d1117";
  ctx.fillRect(0, 0, w, h);

  const padL = 36;
  const padR = 8;
  const padT = 8;
  const padB = 22;
  const plotW = w - padL - padR;
  const plotH = h - padT - padB;

  ctx.strokeStyle = "#1f2937";
  ctx.lineWidth   = 1;
  for (let i = 0; i <= 4; ++i) {
    const y = padT + (plotH / 4) * i;
    ctx.beginPath();
    ctx.moveTo(padL, y);
    ctx.lineTo(padL + plotW, y);
    ctx.stroke();
  }

  if (datasets.length === 0) return;

  let lo = Number.POSITIVE_INFINITY, hi = Number.NEGATIVE_INFINITY;
  for (let i = 0; i < datasets.length; ++i) {
    lo = Math.min(lo, datasets[i].min);
    hi = Math.max(hi, datasets[i].max);
  }
  if (!Number.isFinite(lo) || !Number.isFinite(hi) || hi <= lo) {
    lo = 0; hi = 100;
  }

  ctx.fillStyle    = "#cbd5e1";
  ctx.font         = "10px monospace";
  ctx.textBaseline = "middle";
  ctx.textAlign    = "right";
  for (let i = 0; i <= 4; ++i) {
    const v = hi - ((hi - lo) / 4) * i;
    const y = padT + (plotH / 4) * i;
    ctx.fillText(v.toFixed(1), padL - 4, y);
  }
  ctx.textAlign    = "start";
  ctx.textBaseline = "alphabetic";

  for (let i = 0; i < traces.length; ++i) {
    const t = traces[i];
    if (t.length < 2) continue;

    ctx.strokeStyle = COLORS[i % COLORS.length];
    ctx.lineWidth   = 1.6;
    ctx.beginPath();
    for (let k = 0; k < t.length; ++k) {
      const x = padL + (k / (HISTORY - 1)) * plotW;
      const n = (t[k] - lo) / (hi - lo);
      const y = padT + plotH - n * plotH;
      k === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
    }
    ctx.stroke();
  }

  let lx = padL;
  for (let i = 0; i < datasets.length; ++i) {
    const text = datasets[i].title;
    ctx.fillStyle = COLORS[i % COLORS.length];
    ctx.fillRect(lx, h - padB + 6, 10, 4);
    ctx.fillStyle = "#cbd5e1";
    ctx.font      = "10px sans-serif";
    ctx.fillText(text, lx + 14, h - padB + 14);
    lx += 14 + ctx.measureTextWidth(text) + 14;
  }
}

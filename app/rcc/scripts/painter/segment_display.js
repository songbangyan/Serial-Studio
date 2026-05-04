// 7-segment LCD display.
//
// Renders datasets[0..N-1] as classic 7-segment digits stacked in rows.
// Demonstrates path-based glyph composition without needing a font.

const SEG = {
  // Segments per digit: a, b, c, d, e, f, g
  "0": [1, 1, 1, 1, 1, 1, 0],
  "1": [0, 1, 1, 0, 0, 0, 0],
  "2": [1, 1, 0, 1, 1, 0, 1],
  "3": [1, 1, 1, 1, 0, 0, 1],
  "4": [0, 1, 1, 0, 0, 1, 1],
  "5": [1, 0, 1, 1, 0, 1, 1],
  "6": [1, 0, 1, 1, 1, 1, 1],
  "7": [1, 1, 1, 0, 0, 0, 0],
  "8": [1, 1, 1, 1, 1, 1, 1],
  "9": [1, 1, 1, 1, 0, 1, 1],
  "-": [0, 0, 0, 0, 0, 0, 1],
  " ": [0, 0, 0, 0, 0, 0, 0]
};

function drawDigit(ctx, x, y, w, h, ch, color) {
  const segs = SEG[ch] || SEG[" "];
  const t    = Math.max(2, h * 0.08);
  const onCol = color;
  const offCol = "rgba(255, 255, 255, 0.05)";

  function bar(px, py, pw, ph, on) {
    ctx.fillStyle = on ? onCol : offCol;
    ctx.fillRect(px, py, pw, ph);
  }

  bar(x + t, y,                   w - 2 * t, t, segs[0]);  // a
  bar(x + w - t, y + t,           t, h * 0.5 - t * 1.5, segs[1]);  // b
  bar(x + w - t, y + h * 0.5 + t * 0.5, t, h * 0.5 - t * 1.5, segs[2]);  // c
  bar(x + t, y + h - t,           w - 2 * t, t, segs[3]);  // d
  bar(x,     y + h * 0.5 + t * 0.5, t, h * 0.5 - t * 1.5, segs[4]);  // e
  bar(x,     y + t,               t, h * 0.5 - t * 1.5, segs[5]);  // f
  bar(x + t, y + h * 0.5 - t * 0.5, w - 2 * t, t, segs[6]);  // g
}

function paint(ctx, w, h) {
  ctx.fillStyle = "#000";
  ctx.fillRect(0, 0, w, h);

  if (datasets.length === 0) return;

  const rows = datasets.length;
  const rowH = (h - 8) / rows;

  for (let i = 0; i < rows; ++i) {
    const ds = datasets[i];
    const v  = Number.isFinite(ds.value) ? ds.value : 0;
    const txt = v.toFixed(1).padStart(7, " ");
    const yT  = 4 + i * rowH;

    ctx.fillStyle    = "#475569";
    ctx.font         = "10px sans-serif";
    ctx.textBaseline = "top";
    ctx.fillText(ds.title + (ds.units ? " (" + ds.units + ")" : ""), 8, yT);

    const dh = rowH - 22;
    const dw = dh * 0.55;
    const totalW = txt.length * (dw + dw * 0.18);
    const xT = (w - totalW) * 0.5;

    let cx = xT;
    for (let k = 0; k < txt.length; ++k) {
      const ch = txt[k];
      if (ch === ".") {
        ctx.fillStyle = "#22d3ee";
        ctx.fillRect(cx - dw * 0.12, yT + 18 + dh - dw * 0.18, dw * 0.18, dw * 0.18);
        continue;
      }
      drawDigit(ctx, cx, yT + 18, dw, dh, ch, "#22d3ee");
      cx += dw + dw * 0.18;
    }
  }
  ctx.textBaseline = "alphabetic";
}

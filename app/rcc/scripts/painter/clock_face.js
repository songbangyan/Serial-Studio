// Analog clock face.
//
// Renders a clock from datasets[0] (seconds, 0..86399). If no dataset is
// present, falls back to the wall-clock time so the widget always shows
// something useful.

function readTimeOfDay() {
  if (datasets.length > 0) {
    const v = datasets[0].value;
    if (Number.isFinite(v))
      return v;
  }

  const now    = new Date(frame.timestampMs);
  const h      = now.getHours();
  const m      = now.getMinutes();
  const s      = now.getSeconds();
  return h * 3600 + m * 60 + s;
}

function paint(ctx, w, h) {
  ctx.fillStyle = "#0c0c0e";
  ctx.fillRect(0, 0, w, h);

  const cx = w * 0.5;
  const cy = h * 0.5;
  const r  = Math.min(w, h) * 0.45;

  // Dial
  ctx.strokeStyle = "#888";
  ctx.lineWidth   = 2;
  ctx.beginPath();
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.stroke();

  // Hour ticks
  ctx.lineWidth = 2;
  for (let i = 0; i < 12; ++i) {
    const ang = (i / 12) * Math.PI * 2 - Math.PI / 2;
    const x0  = cx + Math.cos(ang) * r * 0.92;
    const y0  = cy + Math.sin(ang) * r * 0.92;
    const x1  = cx + Math.cos(ang) * r;
    const y1  = cy + Math.sin(ang) * r;
    ctx.beginPath();
    ctx.moveTo(x0, y0);
    ctx.lineTo(x1, y1);
    ctx.stroke();
  }

  const tod  = readTimeOfDay();
  const sec  = tod % 60;
  const min  = (tod / 60) % 60;
  const hour = (tod / 3600) % 12;

  function hand(angle, length, width, color) {
    ctx.strokeStyle = color;
    ctx.lineWidth   = width;
    ctx.lineCap     = "round";
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.lineTo(cx + Math.cos(angle) * length, cy + Math.sin(angle) * length);
    ctx.stroke();
  }

  hand((hour / 12) * Math.PI * 2 - Math.PI / 2, r * 0.5, 5, "#e5e7eb");
  hand((min  / 60) * Math.PI * 2 - Math.PI / 2, r * 0.7, 3, "#e5e7eb");
  hand((sec  / 60) * Math.PI * 2 - Math.PI / 2, r * 0.8, 1, "#ef4444");

  ctx.fillStyle = "#ef4444";
  ctx.beginPath();
  ctx.arc(cx, cy, 3, 0, Math.PI * 2);
  ctx.fill();
}

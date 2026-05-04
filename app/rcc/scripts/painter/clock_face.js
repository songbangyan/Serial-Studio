// Analog clock face.
//
// Renders a clock from datasets[0] (seconds, 0..86399). If no dataset is
// present, falls back to wall-clock time so the widget always shows
// something meaningful.

function readTimeOfDay() {
  if (datasets.length > 0) {
    const v = datasets[0].value;
    if (Number.isFinite(v)) return v;
  }
  const now = new Date(frame.timestampMs);
  return now.getHours() * 3600 + now.getMinutes() * 60 + now.getSeconds();
}

function paint(ctx, w, h) {
  // Cream paper background.
  ctx.fillStyle = "#f5f5f1";
  ctx.fillRect(0, 0, w, h);
  ctx.strokeStyle = "#e7e5de";
  ctx.lineWidth = 2;
  ctx.strokeRect(1, 1, w - 2, h - 2);

  const cx = w * 0.5;
  const cy = h * 0.5;
  const r  = Math.min(w, h) * 0.42;

  // Dial face -- white card.
  ctx.fillStyle = "#ffffff";
  ctx.beginPath();
  ctx.moveTo(cx + r, cy);
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.fill();

  // Outer rim.
  ctx.strokeStyle = "#475569";
  ctx.lineWidth   = 3;
  ctx.beginPath();
  ctx.moveTo(cx + r, cy);
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.stroke();

  // Hour numerals.
  ctx.fillStyle    = "#0f172a";
  ctx.font         = "bold 14px serif";
  ctx.textAlign    = "center";
  ctx.textBaseline = "middle";
  for (let i = 1; i <= 12; ++i) {
    const ang = (i / 12) * Math.PI * 2 - Math.PI / 2;
    const tx  = cx + Math.cos(ang) * r * 0.82;
    const ty  = cy + Math.sin(ang) * r * 0.82;
    ctx.fillText("" + i, tx, ty);
  }

  // Minute ticks.
  ctx.strokeStyle = "#94a3b8";
  for (let i = 0; i < 60; ++i) {
    const ang   = (i / 60) * Math.PI * 2 - Math.PI / 2;
    const major = (i % 5 === 0);
    const inner = r * (major ? 0.93 : 0.96);
    const outer = r * 0.99;
    ctx.lineWidth = major ? 2 : 1;
    ctx.beginPath();
    ctx.moveTo(cx + Math.cos(ang) * inner, cy + Math.sin(ang) * inner);
    ctx.lineTo(cx + Math.cos(ang) * outer, cy + Math.sin(ang) * outer);
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

  hand((hour / 12) * Math.PI * 2 - Math.PI / 2, r * 0.50, 6, "#0f172a");
  hand((min  / 60) * Math.PI * 2 - Math.PI / 2, r * 0.72, 4, "#0f172a");
  hand((sec  / 60) * Math.PI * 2 - Math.PI / 2, r * 0.85, 1, "#dc2626");

  // Centre cap.
  ctx.fillStyle = "#dc2626";
  ctx.beginPath();
  ctx.moveTo(cx + 4, cy);
  ctx.arc(cx, cy, 4, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = "#0f172a";
  ctx.beginPath();
  ctx.moveTo(cx + 2, cy);
  ctx.arc(cx, cy, 2, 0, Math.PI * 2);
  ctx.fill();

  ctx.lineCap = "butt";
  ctx.textAlign = "start";
  ctx.textBaseline = "alphabetic";
}

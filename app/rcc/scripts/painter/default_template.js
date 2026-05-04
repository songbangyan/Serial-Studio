// Default painter template.
//
// Renders datasets[0..2] as an X/Y/Z position indicator. The dot's planar
// position follows (X, Y); its size and colour follow Z. Edit this script
// freely -- it's a starting point.
//
// Available globals:
//   group              { id, title, columns, sourceId }
//   datasets           array-like; datasets[i] -> { value, rawValue, title, units, min, max, ... }
//   datasets.length    dataset count for this group
//   frame              { number, timestampMs }
//   datasetGetRaw(uid) cross-group dataset lookup (raw value)
//   datasetGetFinal(uid)  cross-group lookup (post-transform value)
//   tableGet/tableSet  Data Tables read/write
//
// You can also define `function onFrame() { ... }` to advance state once per
// parsed frame (history buffers, smoothing, etc.) before paint() runs.

function paint(ctx, w, h) {
  // Background
  ctx.fillStyle = "#0d1117";
  ctx.fillRect(0, 0, w, h);

  // Title
  ctx.fillStyle = "#cbd5e1";
  ctx.font      = "bold 14px sans-serif";
  ctx.fillText(group.title || "Painter", 12, 22);

  // Need at least X and Y to draw the indicator
  if (datasets.length < 2) {
    ctx.fillStyle = "#94a3b8";
    ctx.font      = "12px monospace";
    ctx.fillText("Add at least 2 datasets (X, Y) to see the indicator.", 12, 44);
    return;
  }

  const dsX = datasets[0];
  const dsY = datasets[1];
  const dsZ = datasets.length >= 3 ? datasets[2] : null;

  // Plot box, leaving room for axes labels and the side panel
  const padL = 36;
  const padR = 140;
  const padT = 36;
  const padB = 28;
  const plotW = Math.max(1, w - padL - padR);
  const plotH = Math.max(1, h - padT - padB);
  const cx    = padL + plotW * 0.5;
  const cy    = padT + plotH * 0.5;

  // Frame
  ctx.strokeStyle = "#1f2937";
  ctx.lineWidth   = 1;
  ctx.strokeRect(padL, padT, plotW, plotH);

  // Cross-hair grid
  ctx.beginPath();
  ctx.moveTo(padL, cy);
  ctx.lineTo(padL + plotW, cy);
  ctx.moveTo(cx, padT);
  ctx.lineTo(cx, padT + plotH);
  ctx.stroke();

  // Map X/Y values to plot coordinates
  function norm(v, lo, hi) {
    if (!Number.isFinite(v) || hi <= lo) return 0.5;
    return Math.max(0, Math.min(1, (v - lo) / (hi - lo)));
  }

  const nx = norm(dsX.value, dsX.min, dsX.max);
  const ny = norm(dsY.value, dsY.min, dsY.max);
  const px = padL + nx * plotW;
  const py = padT + (1 - ny) * plotH;

  // Z drives both colour (cool -> warm) and dot size
  let nz = 0.5;
  let zText = "—";
  if (dsZ && Number.isFinite(dsZ.value)) {
    nz = norm(dsZ.value, dsZ.min, dsZ.max);
    zText = dsZ.value.toFixed(2) + (dsZ.units ? " " + dsZ.units : "");
  }
  const hue  = 220 - nz * 220;        // 220° (blue) -> 0° (red)
  const dotR = 6 + nz * 14;

  // Soft halo
  ctx.fillStyle = `hsla(${hue}, 80%, 55%, 0.18)`;
  ctx.beginPath();
  ctx.arc(px, py, dotR * 2.6, 0, Math.PI * 2);
  ctx.fill();

  // Filled dot
  ctx.fillStyle = `hsl(${hue}, 80%, 55%)`;
  ctx.beginPath();
  ctx.arc(px, py, dotR, 0, Math.PI * 2);
  ctx.fill();

  // Outline + center pip
  ctx.strokeStyle = "#0d1117";
  ctx.lineWidth   = 2;
  ctx.beginPath();
  ctx.arc(px, py, dotR, 0, Math.PI * 2);
  ctx.stroke();

  ctx.fillStyle = "#fff";
  ctx.beginPath();
  ctx.arc(px, py, 2, 0, Math.PI * 2);
  ctx.fill();

  // Axes labels (bottom + left)
  ctx.fillStyle    = "#94a3b8";
  ctx.font         = "11px monospace";
  ctx.textAlign    = "center";
  ctx.textBaseline = "top";
  ctx.fillText(dsX.title + (dsX.units ? " (" + dsX.units + ")" : ""),
               cx, padT + plotH + 6);

  ctx.save();
  ctx.translate(padL - 24, cy);
  ctx.rotate(-Math.PI * 0.5);
  ctx.textAlign    = "center";
  ctx.textBaseline = "middle";
  ctx.fillText(dsY.title + (dsY.units ? " (" + dsY.units + ")" : ""), 0, 0);
  ctx.restore();
  ctx.textAlign    = "start";
  ctx.textBaseline = "alphabetic";

  // Side panel: live values
  const panelX = padL + plotW + 16;
  const panelY = padT;

  ctx.fillStyle = "#cbd5e1";
  ctx.font      = "11px sans-serif";
  ctx.fillText("X", panelX, panelY + 12);
  ctx.fillText("Y", panelX, panelY + 36);
  if (dsZ) ctx.fillText("Z", panelX, panelY + 60);

  ctx.fillStyle = "#fff";
  ctx.font      = "bold 13px monospace";
  ctx.fillText((Number.isFinite(dsX.value) ? dsX.value.toFixed(2) : "—") +
               (dsX.units ? " " + dsX.units : ""), panelX + 16, panelY + 12);
  ctx.fillText((Number.isFinite(dsY.value) ? dsY.value.toFixed(2) : "—") +
               (dsY.units ? " " + dsY.units : ""), panelX + 16, panelY + 36);
  if (dsZ)
    ctx.fillText(zText, panelX + 16, panelY + 60);
}

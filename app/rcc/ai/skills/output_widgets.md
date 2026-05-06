# Output Widgets

Output widgets (Pro) are interactive controls that transmit bytes back to
the device. Each one belongs to a group and has its own JavaScript
"transmit function" that converts UI state into bytes.

## Five widget types

Output widgets live in a group's output panel. Each has a `type`:

- **0 = Button**: click-to-send. Most common. Use for command triggers
  ("home", "calibrate", "reset").
- **1 = Slider**: a value in `[minValue, maxValue]` with `stepSize` and
  `initialValue`. Sends on drag-end.
- **2 = Toggle**: boolean. Sends one of two payloads depending on state.
- **3 = TextField**: free-form string input + Send button.
- **4 = Knob**: rotary equivalent of slider; same numeric range fields.

## Anatomy

```
project.outputWidget.add{groupId, type}     // create
project.outputWidget.update{groupId, widgetId, title?, icon?,
                            transmitFunction?}  // configure
project.outputWidget.get{groupId, widgetId}    // read current config
```

Each widget has:

- `title`, `icon` — UI labels
- `minValue`, `maxValue`, `stepSize`, `initialValue` — numeric range
  (sliders, knobs)
- `transmitFunction` — JS source that returns the bytes to send

## The transmit function

The function name is `transmit`. It receives the widget's current value
(numeric for slider/knob, boolean for toggle, string for textfield, or a
sentinel for buttons) and returns either a string (sent verbatim) or a
byte array.

```js
// Simple: send a Modbus-shaped command on button click
function transmit() {
  return ":01030000000AF1\r\n";
}

// Slider: send a 16-bit big-endian value
function transmit(value) {
  const v = Math.round(value) & 0xFFFF;
  return new Uint8Array([0xA5, (v >> 8) & 0xFF, v & 0xFF]);
}

// Toggle: send different payload per state
function transmit(checked) {
  return checked ? "RELAY ON\n" : "RELAY OFF\n";
}

// TextField: prepend AT prefix
function transmit(text) {
  return "AT+" + text + "\r\n";
}
```

## Iteration workflow

1. Get current config: `project.outputWidget.get{groupId, widgetId}` —
   preserve user-set ranges/labels when rewriting `transmitFunction`.
2. Read scripting reference:
   `meta.fetchScriptingDocs{kind: "output_widget_js"}`.
3. Adapt a real example: `scripts.list{kind: "output_widget_js"}` ships
   helpers for CRC16/CRC32, NMEA checksums, Modbus framing, SLCAN, GRBL,
   GCode, SCPI, binary packet builders.
4. Push: `project.outputWidget.update{groupId, widgetId,
   transmitFunction: "..."}`.

## Pinning to a workspace

Output widgets render in the OutputPanel (DashboardWidget enum 15). To
pin a panel to a workspace:

```
project.workspace.setCustomizeMode{enabled: true}
project.workspace.addWidget{workspaceId, widgetType: 15,
                            groupId, relativeIndex: 0}
```

The OutputPanel shows ALL output widgets in that group on one tile.
Don't add one tile per output widget — that's not how it's structured.

## Hardware writes are gated

Every transmit is a hardware write. The runtime tags it AlwaysConfirm,
which means the user is shown the exact bytes and must approve EACH
press, even when auto-approve is on. Don't try to bypass this; the
shielding is intentional.

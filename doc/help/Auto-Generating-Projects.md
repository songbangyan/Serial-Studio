# Auto-Generating Projects

For two of the most painful protocol configurations, Serial Studio can build the project for you from a vendor file. Drop in a `.dbc` for CAN Bus or a register-map CSV/XML/JSON for Modbus, and you get back a complete `.ssproj` with groups, datasets, dashboard widgets, and a working frame parser. No hand-typed register addresses, no copy-paste from PDFs.

Both importers are Pro features and live in the **Setup Panel** of the relevant driver.

## Why this exists

Hand-configuring an industrial protocol is the slow part of using Serial Studio. A typical PLC has dozens of holding registers, each with a name, data type, scale factor, and units. A typical automotive ECU has hundreds of CAN signals across multiple message IDs. Typing those into the Project Editor one at a time is the kind of work that takes a full afternoon and leaves you uncertain you got it right.

The shortcut is to start from the file the vendor already wrote. CAN networks ship with `.dbc` files. Modbus devices ship with register tables. Serial Studio reads those directly.

## What you get

Both importers produce the same shape of output:

- A new project (`.ssproj`) with one or more **groups** representing logical chunks of the device (a CAN message, a contiguous Modbus register block).
- A **dataset per signal/register**, complete with `title`, `units`, `min`/`max`, scale, and a sensible default widget (gauge, bar, plot, LED, accelerometer, GPS, depending on what the importer can infer).
- A generated **Lua frame parser** that decodes each frame into the right datasets without any user code.
- For Modbus, the **register groups** are also pushed into the live Modbus driver so it polls the right addresses immediately.

You get a project that's already wired up. From there you arrange widgets in workspaces, tweak titles, and connect.

## At a glance

| Importer        | Source format            | Driver pre-load    | Output                                          |
|-----------------|--------------------------|--------------------|-------------------------------------------------|
| **DBC**         | `.dbc`                   | No                 | Project + Lua parser, signals grouped by CAN ID |
| **Modbus map**  | `.csv` / `.xml` / `.json`| Yes (register groups) | Project + Lua parser, registers grouped by type/contiguity |

Both importers preview before committing. You see the parsed messages or registers, click **Create Project**, and a `.ssproj` is generated and opened in the Project Editor.

## DBC import (CAN Bus)

A DBC (CAN Database) file describes every message and signal on a CAN network: message IDs, signal bit positions, byte order, scaling, units, and value tables. Most automotive and industrial CAN networks ship with one. Serial Studio's importer parses it with Qt's `QCanDbcFileParser` and turns each message into a group.

### How to run it

1. In the **Setup Panel**, select **CAN Bus** from the I/O Interface dropdown.
2. Configure the CAN backend, interface, and bitrate as usual (see [Protocol Setup Guides](Protocol-Setup-Guides.md)).
3. Click **Import DBC File...**.
4. Pick a `.dbc`. Serial Studio parses it and shows a preview listing every message: `1: EngineData @ 0x100 (5 signals)`, `2: VehicleSpeed @ 0x101 (2 signals)`, and so on.
5. Click **Create Project**. You'll be prompted for an output path.

Messages are sorted by CAN ID so dataset positions stay stable across re-imports.

### What it generates

For each CAN message:

- One **group** named after the message (e.g. *EngineData*).
- One **dataset per signal** with the signal's name, units, and a derived widget choice.
- A dispatch entry in the generated **Lua frame parser** that selects the message by CAN ID, extracts each signal at the correct bit offset and byte order, applies factor and offset, and writes the result into the dataset.

### Widget inference

The DBC importer is fairly aggressive about picking the right widget. It looks at signal names and groupings to detect:

- **GPS coordinates** (latitude/longitude pairs) → GPS Map widget.
- **Accelerometer / gyroscope** triplets (X/Y/Z) → 3D motion widgets.
- **Wheel speeds**, **tire pressures**, **temperature arrays**, **voltage arrays**, **battery cell clusters** → grouped multi-plot or bar widgets.
- **Status flags** and single-bit signals → LEDs.
- Anything else → a plot or gauge based on numeric range.

If the inference picks the wrong widget, change it in the Project Editor. The generated parser doesn't care which widget renders the value.

### Mux signals

Simple multiplexing is supported. When a message declares a `MultiplexorSwitch` (the selector signal in the DBC) plus one or more `MultiplexedSignal` entries, the importer:

- Creates one dataset for the selector itself, titled `<name> (selector)`.
- Creates one dataset per muxed signal, titled `<name> (mux N)` so you can distinguish payloads that share the same bits.
- Generates a Lua decoder that extracts the selector first and gates each muxed signal on `if raw_<selector> == N then ... end`. When the selector doesn't match, the muxed dataset retains its previous value rather than decoding noise from another payload.

Extended multiplexing (DBC `SG_MUL_VAL_` rows, `SwitchAndSignal` intermediates, multi-range parents) is not supported. Those signals are skipped during import; the post-import dialog tells you how many were dropped. To handle them, edit the generated Lua parser by hand or write a custom one.

## Modbus map import

Modbus register maps come in a hundred ad-hoc formats. Serial Studio accepts the three most common ones (CSV, XML, JSON), auto-detects which one it is from the file extension, and groups contiguous registers of the same type into one **register group** for efficient polling.

### How to run it

1. In the **Setup Panel**, select **Modbus** from the I/O Interface dropdown.
2. Click **Import Register Map...**.
3. Pick a `.csv`, `.xml`, or `.json` file.
4. Review the registers in the preview dialog: `0: Temperature @ holding 0x0000 (uint16, °C)`, `1: Pressure @ holding 0x0001 (uint16, PSI)`, ...
5. Click **Create Project**. You'll be prompted to save the `.ssproj`.
6. The contiguous register blocks are also pushed into the live Modbus driver, so polling starts immediately when you connect.

### What it generates

For each contiguous block of same-type registers (holding, input, coil, discrete input):

- One **group** named after the register type and start address (e.g. *Holding @ 0*).
- One **dataset per register**, with the register's name, units, scale, and offset baked in.
- A **Lua frame parser entry** that decodes each register from the Modbus response (handling endianness for 32-bit and 64-bit values automatically).
- A **register group** added to the Modbus driver itself, sized to cover the contiguous block.

### Format details

The detailed format spec for CSV, XML, and JSON register maps lives in [Protocol Setup Guides → Modbus Register Map Import](Protocol-Setup-Guides.md). Highlights:

- **CSV** with a header row. Column order is flexible: `address`, `name`, `type`, `dataType`, `units`, `min`, `max`, `scale`, `offset`. Common aliases (`addr`, `register`, `data_type`, etc.) are auto-recognized. `#` lines are comments.
- **XML** in either flat (`<register address="0" type="holding" .../>`) or nested (`<holding-registers><register address="0" .../></holding-registers>`) form.
- **JSON** as either an array of registers or an object with per-type arrays.

You can usually get going with just `address` and `name`. Everything else has reasonable defaults.

## When to use auto-generation vs. by hand

Auto-generation is the right call when:

- The vendor ships a `.dbc` or a register-map file and it's reasonably accurate.
- You're prototyping against a device for the first time and want to see signals on the dashboard immediately.
- The device has more than a handful of signals or registers (the cost of typing them in by hand grows linearly).

Build the project by hand when:

- You only care about a few specific signals out of a 200-signal DBC. Importing all of them and then deleting most is more work than adding three datasets manually.
- The device speaks Modbus but the registers don't fit the standard "address, type, name, units" pattern (custom encodings, bit-packed flags inside one register, packed structs).
- You want full control over the frame parser, including custom dispatch logic, conditional decoding, or multi-frame state machines.

## Editing after import

The generated project is just a project. Once it's open in the Project Editor, everything is editable: rename datasets, regroup them, swap widgets, edit the Lua parser, add transforms, attach datasets to workspaces. The importer's job is to get you to a working dashboard fast, not to lock you into a layout.

If you need to re-import (the vendor shipped a new DBC, you added a register), the safest path is to import again as a new project and copy over your dashboard customizations, rather than trying to merge by hand.

## See also

- [Protocol Setup Guides](Protocol-Setup-Guides.md): full format reference for Modbus register maps, plus connection setup for both drivers.
- [Communication Protocols](Communication-Protocols.md): Modbus and CAN Bus protocol overview.
- [Project Editor](Project-Editor.md): where the generated project lands after import; adjust groups, datasets, and widgets there.
- [Drivers — Modbus](Drivers-Modbus.md): protocol primer, including the function-code and register-table model the importer maps onto.
- [Drivers — CAN Bus](Drivers-CAN-Bus.md): protocol primer, including the DBC signal model.
- [Frame Parser Scripting](JavaScript-API.md): for editing the generated parser by hand if you need custom logic.
- [Use Cases](Use-Cases.md): industrial and automotive examples that benefit from auto-generation.

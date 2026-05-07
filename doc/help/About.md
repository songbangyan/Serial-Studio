# About Serial Studio

Background, philosophy, and history of the project. Useful if you want to know who wrote it, why it exists, and how it ended up the shape it is now.

## Author

Serial Studio is built by **Alex Spataru** (`alex@serial-studio.com`). The project started as a one-person tool for satellite and rover ground-station work and is still primarily developed by Alex, with contributions from the open-source community on [GitHub](https://github.com/Serial-Studio/Serial-Studio). Anyone can read the source, file issues, and submit patches under the GPL-3.0; the optional Pro edition funds full-time development.

## The problem that started it

Before Serial Studio, Alex worked on a [CanSat competition](https://cansatcompetition.com/) and later on the [NASA Human Exploration Rover Challenge](https://www.nasa.gov/learning-resources/nasa-human-exploration-rover-challenge/) during college. The CanSat work shipped with its own ground-station software — purpose-built for that competition's telemetry format — and every protocol tweak meant another late night in the dashboard code instead of on the hardware. The pattern repeated across teams and semesters: a microcontroller printed sensor values over a serial cable, and somebody had to build a custom GUI around them.

The first attempt at a generic answer was a personal project called **SigLAB**. It worked. The data model was already protocol-agnostic, and the dashboard was already data-driven. The reason it was eventually renamed wasn't technical: Alex wanted the project to live under its own organization rather than buried in a personal list of side projects, with room for outside contributors and a clean GitHub identity. On October 18, 2020 the SigLAB codebase was imported into a new repository under [github.com/Serial-Studio](https://github.com/Serial-Studio) — the literal first commit message in this repo is *"Initial commit from SigLAB codebase"* — and the project took on its current name.

The launch blog post — [*Introducing Serial Studio*](https://www.aspatru.com/blog/introducing-serial-studio), December 29, 2020 — opens with the line that still describes the project:

> *Did you ever have the need to display data from a microcontroller on a dashboard, and spent more time developing (and fixing) your dashboard software, than actually working on your MCU project?*

## Philosophy

Three ideas have stayed constant since 2020.

1. **One tool, many devices.** You should not have to write a new dashboard every time you swap microcontrollers. Describe the data once in a project file, and Serial Studio takes care of the visualization.
2. **The microcontroller stays simple.** The original blog post offered two modes — *Auto* (the device sends a self-describing JSON frame) and *Manual* (the device sends bare CSV and the dashboard layout lives on the PC) — so embedded code stays small. Modern Serial Studio kept the *Manual* idea and grew it: today's project files configure frame parsing entirely on the PC, so firmware can keep printing comma-separated numbers.
3. **GPL core, optional Pro.** The base build — Serial/UART, TCP/UDP, Bluetooth LE, 15+ widgets, Quick Plot, Project Editor, CSV export, Lua/JavaScript frame parsers — is GPL-3.0 and free forever. The Pro edition adds drivers and features that fund development without ever locking up what was already free. See [Pro vs Free](Pro-vs-Free.md) for the exact split.

## How the project evolved

Serial Studio grew driver-by-driver as users brought new hardware to it. Most additions were not planned in advance — they came from a specific request and got generalized once they worked.

### Connectivity, in the order it landed

- **Serial/UART (October 2020).** The original and still the most common transport. Inherited from SigLAB.
- **Network sockets (early 2021).** TCP first, then UDP — including UDP multicast in October 2021 — for users running Serial Studio on a different machine from the device, or behind a network gateway. This was the moment "Serial Studio" stopped being literally about serial.
- **MQTT (February–March 2021).** Both publisher and subscriber. The trigger was IoT users who wanted to bridge a fleet of devices through a broker rather than running a dedicated TCP listener per node.
- **Bluetooth Low Energy (February 2022).** GATT services and characteristics, with shared discovery state across instances so the device list survives QML rebuilds. Brought in for wireless sensors and prototyping boards (ESP32, nRF52) that don't have a USB cable attached.
- **Audio input (July 2025).** Microphones, line-in, and any OS-level audio device, used as a high-rate analog data source. The audio driver added the FFT pipeline that the rest of the dashboard now reuses, and pushed the codebase to handle 48 kHz+ sample rates without dropping frames.
- **Modbus & CAN Bus (December 2025).** The first deliberately *industrial* drivers. Modbus TCP and RTU, with a register-map importer that turns vendor CSV/XML/JSON into a working project. CAN Bus uses Qt's SocketCAN/PCAN/Vector backends and added a DBC importer so automotive `.dbc` files decode signals automatically.
- **Raw USB via libusb (February 2026).** For devices that expose vendor-specific USB endpoints instead of a CDC serial profile — common on logic analyzers, custom data acquisition boards, and some scientific instruments.
- **HID and Process I/O (March 2026).** HID via hidapi for game controllers, custom HID firmware, and medical devices. Process I/O for piping data in from any external program — `socat`, `nc`, a Python script, anything that writes to stdout. Both shipped together as the "experimental" tier of drivers.

### Visualization and analysis

The widget set started with line plots, gauges, bar charts, and a GPS map in late 2020, and slowly grew into the current set of 15+ widgets. The bigger inflection points were:

- **XY plot** (November 2024) for arbitrary x/y mappings instead of time-on-x.
- **3D plot** (May 2025) — a hand-rolled CPU 2.5D painter, deliberately not Qt3D, so it works without a discrete GPU and still draws thousands of points smoothly.
- **Image View** (live camera feed) and **Painter widget** (a scriptable Canvas2D-style canvas driven by a JavaScript `paint(ctx, w, h)` callback, May 2026) for visualizations that don't fit any of the standard widgets.
- **FFT Plot** and **Waterfall / spectrogram** (April 2026), which share the same per-dataset FFT settings; the waterfall can also run in *Campbell mode*, where rows are placed by another dataset's value (e.g. RPM) for order-tracking analysis.

### The 2.0 / 3.0 rewrite

The jump from 1.x to 2.0 and then 3.0 was effectively a ground-up rewrite of the data path. The IO layer dropped its singletons, `FrameBuilder` and `FrameReader` were redesigned around a lock-free single-producer/single-consumer pipeline, and the dashboard moved to a zero-allocation hot path that targets 256 kHz+ frame rates. The visible feature set didn't change much across that boundary; the invisible one — throughput, memory behavior, threading — changed completely.

## Where the project is now

A set of features added since the early 3.x line has turned Serial Studio from a viewer into a small data platform.

### Data Tables — a shared data bus

Datasets are no longer islands. Every project carries an auto-generated **system table** with `raw:<id>` and `final:<id>` registers for every dataset, plus user-defined tables of `Constant` (project-time) and `Computed` (per-frame) registers. Frame parsers, value transforms, and output widgets all read and write the same registers, which means a sensor calibration written once is reusable everywhere. See [Data Tables](Data-Tables.md).

### Per-dataset value transforms

Each dataset can carry a small Lua or JavaScript snippet — `function transform(value) → number` — that runs every frame. Transforms compile once when the project loads and call into a shared engine per source, so an EMA, a unit conversion, or a calibration curve costs roughly a function call per frame. Transforms can derive **virtual datasets** (no frame index, computed entirely from other registers), which is how things like running averages, rate-of-change, and derived KPIs become first-class datasets. See [Dataset Value Transforms](Dataset-Transforms.md).

### Output (control) widgets

Buttons, toggles, sliders, knobs, text fields, and a freeform Output Panel send data *back* to the device. Each one runs a JavaScript template — GCode, SCPI, Modbus, NMEA, CAN, SLCAN, GRBL, custom binary packets — to convert UI state into bytes. There's a Transmit Test Dialog so you can preview the wire output before firing the real command. See [Output Controls](Output-Controls.md).

### Session Database

Sessions can be recorded into a SQLite `.db` (parsed frames, raw bytes, data-table snapshots, and project metadata, all in one file). The Database Explorer browses, tags, and exports sessions; the SQLite Player replays a stored session through the live FrameBuilder pipeline so dashboards and reports work identically on recorded data. Session Reports turn the same database into a styled PDF with charts and a table-of-contents. See [Session Database](Session-Database.md) and [Session Reports](Session-Reports.md).

### File transmission

XMODEM, YMODEM, and ZMODEM are built in for firmware uploads, log retrieval, and any device that talks one of the classic file-transfer protocols. ZMODEM includes ZRPOS-based crash recovery. See [File Transmission](File-Transmission.md).

### AI integration

Two complementary surfaces:

- **MCP / TCP API on port 7777.** A JSON-RPC API with ~290 commands across 25 handlers, wrapped in the Model Context Protocol so any MCP host (Claude Desktop, custom agents, automation scripts) can read live values, edit projects, control workspaces, manage sessions, and so on. See [API Reference](API-Reference.md) and [gRPC Server](gRPC-Server.md).
- **In-app AI Assistant (Pro).** A bring-your-own-key chat panel that edits the project for you. Five providers are wired in: Anthropic, OpenAI, Google Gemini, DeepSeek, and any OpenAI-compatible local server (Ollama, llama.cpp, LM Studio, vLLM) for fully offline use. Mutating actions show an Approve/Deny card before they touch the project. See [AI Assistant](AI-Assistant.md).

The Assistant uses the same command surface as the MCP API, so anything the API can do, the Assistant can do — and vice versa. Both refuse to touch the running connection (open/close, change baud rate, change Modbus slave) so a wrong answer cannot knock you offline mid-shift.

## Where to learn more

- **Source code & releases.** [github.com/Serial-Studio/Serial-Studio](https://github.com/Serial-Studio/Serial-Studio).
- **Discussions and bug reports.** [GitHub Discussions](https://github.com/Serial-Studio/Serial-Studio/discussions) and [Issues](https://github.com/Serial-Studio/Serial-Studio/issues).
- **The original blog post.** [Introducing Serial Studio](https://www.aspatru.com/blog/introducing-serial-studio), December 29, 2020 — outdated in detail, but the motivation still applies.
- **Author's site.** [aspatru.com](https://www.aspatru.com/).

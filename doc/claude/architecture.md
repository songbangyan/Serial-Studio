# Architecture — Index

The architecture corpus is split per subsystem under `doc/claude/architecture/` so "read the
relevant doc in full" loads only the subsystem being touched. Every fact from the old
monolithic file lives in exactly one file below; this index carries no unique content. The
most dangerous rules (threading, hotpath connections, no-alloc, ctor closure) are also
summarized inline in CLAUDE.md — the sub-files hold the full detail.

| File | Read it in full before touching |
|------|--------------------------------|
| [architecture/dataflow.md](architecture/dataflow.md) | Anything on the Driver → FrameReader → FrameBuilder → Dashboard path: data-flow diagram, timestamp ownership, threading rules, cached hotpath flags (`m_changeDriven`, `m_captureLatestFrame`, ...), the 256 kHz benchmark gate and its CI mechanics. |
| [architecture/startup.md](architecture/startup.md) | ModuleManager, singleton construction order, the ProjectModel ctor-closure protected surface, AppState, operation modes, MMCSS, the packaging-aware updater. |
| [architecture/io.md](architecture/io.md) | IO drivers and managers: the no-singleton-driver model, UI-vs-live driver split, file transmission protocols. New drivers: `ss-new-driver` skill + `BluetoothLE` reference. |
| [architecture/project.md](architecture/project.md) | ProjectModel/ProjectEditor split, file watcher, rolling backups, multi-source, JSON `Keys::` + schema versioning, the Modbus map / DBC importers and their generated parsers + dashboards. |
| [architecture/scripting.md](architecture/scripting.md) | The three parser engines (JS/Lua/Native), watchdogs, per-dataset transforms, data tables, control scripts (worker-thread contract + lifecycle), embedded code-editor plumbing, binary-decoder byte-table semantics. |
| [architecture/dashboard.md](architecture/dashboard.md) | `UI::Dashboard` ingest push tables, alarm bands, dashboard tools, plot X-axis / TimeRing / downsamplers / GPU curves / area fill / sweep-trigger, time range, waterfall, output widgets, workspaces. |
| [architecture/export.md](architecture/export.md) | CSV/MDF4 export schema, the Sessions SQLite DB (schema, replay, snapshots, PK/index rules). |
| [architecture/commands-icons.md](architecture/commands-icons.md) | Before adding a toolbar button, palette entry, menu item, keyboard shortcut, or fixed icon: the spec-0028 icon registry (tiered tree, `IconRegistry` resolution, legacy map) and command registry (JSON command/layout manifests, per-context bindings, `CommandModel`/`CommandToolbar`, contexts + binding-set ordering, shortcuts, translations) with recipes for new commands/surfaces. |

Cross-cutting reads: a Dashboard ingest change is also a hotpath change (dataflow.md); a
ProjectModel ctor-adjacent change is also a startup change (startup.md). When a change spans
subsystems, read every file it touches.

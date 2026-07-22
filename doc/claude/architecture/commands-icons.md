# Icon & Command Registry (spec 0028)

The single source of truth for fixed UI icons and for the commands behind toolbars,
menus, the palette, and shortcuts. Read this before adding a toolbar button, a palette
entry, a menu item, a keyboard shortcut, or a new fixed icon. The goal of the system: a
new command or a whole new toolbar is a data edit plus one small behavior binding, not a
QML surface rewrite.

## Icons

**Tree:** `app/rcc/icons/<category>/<tier>/<name>.svg`. Tiers are `16 24 32 48` (design
px). Categories: `widgets window editor devices panes console database code licensing
notifications commands system`. The `buttons/` folder is exempt (kept as-is, direct
paths). The `system` category holds `missing.svg` (the placeholder).

**Resolve, never hardcode a path:**
- QML: `Cpp_Misc_IconRegistry.icon("<category>", "<name>", px)` returns a `qrc:/…` URL;
  `Cpp_Misc_IconRegistry.iconById("<category>/<name>", px)` takes the combined id used by
  model roles.
- C++: `Misc::IconRegistry::instance().icon(cat, name, px)` (QML/`qrc:`),
  `.iconPath(cat, name, px)` (C++ `:/…` for QPixmap/QIcon/QSvgRenderer), or
  `.iconById("cat/name", px)`.
- Resolution rule: nearest tier **at or above** `px`, else the largest available. An
  unknown id logs one warning and serves `system/16/missing.svg` (never a crash, never a
  silent blank).

**Adding an icon:** drop `name.svg` into the right `<category>/<tier>/` folder(s), add the
`<file>` line to `app/rcc/rcc.qrc`, done — no C++/enum/QML table. Run
`scripts/registry-verify.py` (checks layout, duplicates, qrc↔disk sync).

**Widget icons:** `SerialStudio::dashboardWidgetIcon(w, large)` maps the widget enum to a
`widgets/<name>` id and resolves it (16/32). The taskbar model publishes a logical `iconId`
role alongside the resolved-URL role so each surface resolves at its own display size
(taskbar 16, palette cells 32) from one model — user-picked workspace icons keep the
`Misc::IconEngine` inline-SVG path and leave `iconId` empty.

**Legacy paths:** old `qrc:/icons/…` URLs persisted in user project files are remapped by
`Misc::legacyIconPath()` (generated table, consulted in `IconEngine::resolveActionIconSource`).
Regenerate with `scripts/generate-legacy-icons.py` only if the migration manifest changes.

## Commands

A command is declared once (metadata) and bound once per context (behavior). Nothing
about a command lives inside a toolbar/menu QML file anymore.

### Manifests (metadata + layout) — `app/rcc/commands/`

- **Command manifests** `app.json`, `dashboard.json`, `projecteditor.json`, `database.json`.
  Each command: `id` (`scope.verb`), `title`, `tooltip?`, `icon` (`"category/name"`),
  `titleChecked?`/`iconChecked?` (toggles), `shortcut?`, `shortcutWindows?`,
  `kind` (`action`|`toggle`), `pro?` (dropped from the catalog on GPL builds),
  `contexts` (which palettes list it — see below), `order?` (palette sort),
  `category` (palette grouping key, one of file/mode/connection/view/export/console/
  project/license/tools/help — validated by `registry-verify`; display names + section
  order live in `PaletteModel.categoryLabel`/`categoryOrder`).
- **Layout manifests** `layouts/{main-toolbar,project-toolbar,start-menu,database-toolbar}.json`.
  Node types: `section` (with `collapsible`/`collapsiblePro`/`collapsePriority`/
  `showSeparator`/`collapsedCommands`), `command` (`id` + optional `title`/`icon`/`tooltip`
  overrides + `role`), `grid` (`rows`, `itemHints`, `role: "drivers"` for the build-variant
  driver grid), `separator`, `slot` (a named hole the host fills, e.g. Start-menu
  Workspaces/Actions/Plugins), `submenu`.

`UI::CommandRegistry` (`Cpp_UI_CommandRegistry`) loads and validates all of them, filters
`pro` on GPL, translates titles/tooltips at query time (see Translations), and re-emits
`commandsChanged` on language switch. Add a new manifest/layout by adding a `loadManifest`/
`loadLayout` line in its ctor + the `<file>` in `rcc.qrc`.

### Behavior — `app/qml/Commands/*CommandBindings.qml`

One `QtObject` per context with a `readonly property var map: ({ "<id>": root.cmdX, … })`
and one `readonly property QtObject cmdX` per id holding only `run()` plus optional
`enabled`/`visible`/`checked`/`tooltip`. Metadata (title/icon/shortcut) is NOT here.
`AppCommandBindings`, `DashboardCommandBindings`, `ProjectEditorCommandBindings` live in
`Commands/`; a commercial-only surface keeps its bindings beside its own QML instead (e.g.
`DatabaseExplorer/DatabaseCommandBindings.qml`) so the `registry-verify` commercial-guard
scan of `Commands/*.qml` does not false-positive on `Cpp_Sessions_`/`Cpp_MQTT_` refs.

**Commercial guard:** in `Commands/*.qml`, any line referencing `Cpp_Licensing_`/
`Cpp_Sessions_`/`Cpp_MQTT_` must carry `Cpp_CommercialBuild` on the same or previous line
(short-circuit patterns) — enforced by `registry-verify.py` so GPL builds never evaluate
Pro symbols.

### Join + render

- `Commands/CommandModel.qml` joins registry metadata with a context's binding set(s):
  `CommandModel { context: "app"; bindingSets: [appBindings, dashBindings] }`. `items(query)`
  returns provider-shaped entries (`name/icon/iconId/run/checked/shortcut/visible/enabled`);
  `binding(id)`/`entryFor(id)` are the per-id lookups. A command with no binding in the set
  is silently dropped, so **every id you tag for a context needs a binding reachable by that
  context's model**.
- `Widgets/CommandToolbar.qml` renders a ribbon from a layout surface:
  `CommandToolbar { model: aCommandModel; surface: "main-toolbar" }`. It builds
  `RibbonSection`s/`ToolbarButton`s/grids from the layout tree with live behavior; hosts keep
  only their chrome and any right-pinned buttons (e.g. Connect/Activate). Start menu and the
  palette consume the same models via their own delegates.

### Contexts and the two-binding-set pattern

`contexts` on a command decides which palettes list it: `"app"` (main window, not
connected), `"dashboard"` (dashboard/workspace-switcher palette), `[]` (toolbar/shortcut
only, never in a palette). A command can be in both.

Because `AppCommandBindings` and `DashboardCommandBindings` both define `dashboard.autoLayout`/
`dashboard.freeze` with different bodies (main-window delegates to the visible pane; dashboard
acts directly), **binding-set order matters** — `CommandModel.binding` returns the first set
that has the id:
- Main-window palette + toolbar: `[app, dashboard]` (app first).
- Dashboard palette + toolbar: `[dashboard, app]` (dashboard first).

This lets both palettes show the union of commands while each keeps the correct
implementation of the shared ids.

### Shortcuts

Sequences live in the command manifest (`shortcut` + `shortcutWindows`). Each host window
runs one `Instantiator` over `Cpp_UI_CommandRegistry.shortcutCommands("<window>")`,
gating on the binding's `enabled` (never `visible`). A few chords that can't be one command
(Tab / Ctrl+1..9 families) stay as hand-written `Shortcut` blocks — see
`doc/claude/specs/0028-icon-registry/shortcut-checklist.md`.

### Translations

Command titles/tooltips are translated through `QCoreApplication::translate("Commands", …)`.
`scripts/generate-command-strings.py` emits `app/src/UI/CommandStrings.cpp`
(a `QT_TRANSLATE_NOOP` stub) so lupdate sees them; it runs inside `sanitize-commit.py` and
`--check` gates drift. Dynamic per-state strings (a toggle's changing tooltip) use `qsTr`
in the binding.

## Recipes

- **New toolbar button:** add the command to the context manifest, add a `command` node to
  the layout manifest, add a `cmd…` binding entry. It appears with icon/tooltip/shortcut/
  translation wired.
- **New palette command:** add the command with the right `contexts`, ensure a binding
  exists in every context's model (`registry-verify` + the binding-coverage check catch a
  miss).
- **New toolbar surface (like the Session DB):** new command manifest + layout manifest +
  bindings file, a `loadManifest`/`loadLayout` pair, qrc entries, and swap the host's
  hand-built ribbon for `CommandToolbar { model; surface }`.
- **Verify:** `scripts/registry-verify.py` (tree + manifests + guard scan),
  `scripts/generate-command-strings.py --check`, `scripts/code-verify.py --check`.

#!/usr/bin/env python3
"""Structural lint for the spec-0028 icon registry tree and its qrc block.

Checks the migrated icon tree (`app/rcc/icons/`) against the registry contract:
every file sits at `<category>/<tier>/<name>.svg` with a known category and a
tier in {16, 24, 32, 48}; no byte-identical duplicates exist outside the exempt
`buttons/` set (spec AC2); rcc.qrc and the disk tree agree in both directions;
and every compat alias points at an existing file. The alias report counts live
source references to each old path -- task T21 drops the alias block only when
`--require-no-alias-refs` passes. Exit code 0 = clean, 1 = violations.

Usage:
    python3 scripts/registry-verify.py [--alias-report] [--require-no-alias-refs]
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RCC = ROOT / "app" / "rcc"
ICONS = RCC / "icons"
QRC = RCC / "rcc.qrc"

EXEMPT_FOLDERS = {"buttons"}
CATEGORIES = {
    "widgets",
    "window",
    "editor",
    "devices",
    "panes",
    "console",
    "database",
    "code",
    "licensing",
    "notifications",
    "commands",
    "system",
}
TIERS = {"16", "24", "32", "48"}

SOURCE_ROOTS = (ROOT / "app" / "qml", ROOT / "app" / "src")
SOURCE_EXTS = {".qml", ".cpp", ".h"}

NAME_RE = re.compile(r"^[a-z0-9][a-z0-9-]*\.svg$")


def fail(errors: list[str], message: str) -> None:
    errors.append(message)


def check_tree(errors: list[str]) -> None:
    hashes: dict[str, list[str]] = defaultdict(list)
    for path in sorted(ICONS.rglob("*")):
        if path.is_dir():
            continue
        rel = path.relative_to(ICONS).as_posix()
        top = rel.split("/", 1)[0]
        if top in EXEMPT_FOLDERS or path.name == ".DS_Store":
            continue
        parts = rel.split("/")
        if path.suffix != ".svg":
            fail(errors, f"non-SVG file in icon tree: icons/{rel}")
            continue
        hashes[hashlib.md5(path.read_bytes()).hexdigest()].append(rel)
        if len(parts) != 3:
            fail(errors, f"not <category>/<tier>/<name>.svg: icons/{rel}")
            continue
        category, tier, name = parts
        if category not in CATEGORIES:
            fail(errors, f"unknown category '{category}': icons/{rel}")
        if tier not in TIERS:
            fail(errors, f"tier not in {sorted(TIERS)}: icons/{rel}")
        if not NAME_RE.match(name):
            fail(errors, f"name not kebab-case: icons/{rel}")
    for digest, paths in sorted(hashes.items()):
        if len(paths) > 1:
            fail(errors, f"byte-identical duplicates: {', '.join(paths)}")


def qrc_entries() -> tuple[set[str], dict[str, str]]:
    text = QRC.read_text(encoding="utf-8")
    real = set(re.findall(r"<file>(icons/[^<]+)</file>", text))
    aliases = dict(
        re.findall(r'<file alias="(icons/[^"]+)">(icons/[^<]+)</file>', text)
    )
    return real, aliases


def check_qrc(errors: list[str]) -> dict[str, str]:
    real, aliases = qrc_entries()
    disk = {f"icons/{p.relative_to(ICONS).as_posix()}" for p in ICONS.rglob("*.svg")}
    for entry in sorted(real - disk):
        fail(errors, f"qrc entry has no file on disk: {entry}")
    for entry in sorted(disk - real):
        fail(errors, f"file on disk not registered in qrc: {entry}")
    for alias, target in sorted(aliases.items()):
        if not (RCC / target).exists():
            fail(errors, f"alias points at missing file: {alias} -> {target}")
        if alias in disk:
            fail(errors, f"alias shadows a real file: {alias}")
    return aliases


MANIFESTS = ["app.json", "dashboard.json", "projecteditor.json", "database.json"]
LAYOUTS = [
    "main-toolbar.json",
    "project-toolbar.json",
    "start-menu.json",
    "database-toolbar.json",
]
KNOWN_CONTEXTS = {"app", "dashboard", "editor"}
KNOWN_KINDS = {"action", "toggle"}
KNOWN_WINDOWS = {"main", "editor"}
KNOWN_STANDARD_KEYS = {
    "Open",
    "New",
    "Save",
    "Quit",
    "Back",
    "Forward",
    "Close",
    "Preferences",
}
COMMERCIAL_SYMBOLS = ("Cpp_Licensing_", "Cpp_Sessions_", "Cpp_MQTT_")
KNOWN_CATEGORIES = {
    "file",
    "mode",
    "connection",
    "view",
    "export",
    "console",
    "project",
    "license",
    "tools",
    "help",
}


def icon_ref_resolves(ref: str) -> bool:
    category, _, name = ref.partition("/")
    if not category or not name:
        return False
    return any(
        (ICONS / category / str(t) / f"{name}.svg").exists() for t in (16, 24, 32, 48)
    )


def check_manifests(errors: list[str]) -> set[str]:
    ids: set[str] = set()
    shortcuts: dict[tuple[str, str], str] = {}
    for name in MANIFESTS:
        path = RCC / "commands" / name
        if not path.exists():
            fail(errors, f"missing command manifest: {name}")
            continue
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            fail(errors, f"invalid JSON in {name}: {error}")
            continue
        for command in data.get("commands", []):
            cid = command.get("id", "")
            if not cid or cid in ids:
                fail(errors, f"{name}: missing or duplicate id '{cid}'")
                continue
            ids.add(cid)
            if command.get("kind", "action") not in KNOWN_KINDS:
                fail(errors, f"{name}: {cid} has unknown kind")
            cat = command.get("category")
            if cat is not None and cat not in KNOWN_CATEGORIES:
                fail(errors, f"{name}: {cid} has unknown category '{cat}'")
            for ctx in command.get("contexts", []):
                if ctx not in KNOWN_CONTEXTS:
                    fail(errors, f"{name}: {cid} has unknown context '{ctx}'")
            for key in ("icon", "iconChecked"):
                ref = command.get(key)
                if ref and not icon_ref_resolves(ref):
                    fail(errors, f"{name}: {cid} {key} '{ref}' does not resolve")
            shortcut = command.get("shortcut", "")
            if shortcut.startswith("StandardKey."):
                if shortcut.split(".", 1)[1] not in KNOWN_STANDARD_KEYS:
                    fail(
                        errors,
                        f"{name}: {cid} unknown {shortcut} (extend the C++ table)",
                    )
            for window in command.get("shortcutWindows", []):
                if window not in KNOWN_WINDOWS:
                    fail(errors, f"{name}: {cid} unknown shortcut window '{window}'")
                if shortcut:
                    key = (window, shortcut)
                    if key in shortcuts:
                        fail(
                            errors,
                            f"duplicate shortcut {shortcut} in window {window}: "
                            f"{shortcuts[key]} and {cid}",
                        )
                    shortcuts[key] = cid
    return ids


def check_layout_nodes(
    errors: list[str], name: str, nodes: list, ids: set[str]
) -> None:
    for node in nodes:
        if node.get("type") == "command" and node.get("id") not in ids:
            fail(
                errors, f"{name}: layout references unknown command '{node.get('id')}'"
            )
        for ref in node.get("collapsedCommands", []):
            if ref not in ids:
                fail(errors, f"{name}: collapsedCommands references unknown '{ref}'")
        ref = node.get("icon")
        if ref and not icon_ref_resolves(ref):
            fail(errors, f"{name}: layout icon '{ref}' does not resolve")
        check_layout_nodes(errors, name, node.get("items", []), ids)


def check_layouts(errors: list[str], ids: set[str]) -> None:
    for name in LAYOUTS:
        path = RCC / "commands" / "layouts" / name
        if not path.exists():
            fail(errors, f"missing layout manifest: {name}")
            continue
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            fail(errors, f"invalid JSON in {name}: {error}")
            continue
        for key in ("sections", "items", "pinnedEnd"):
            check_layout_nodes(errors, name, data.get(key, []), ids)


def check_binding_guards(errors: list[str]) -> None:
    bindings_dir = ROOT / "app" / "qml" / "Commands"
    if not bindings_dir.is_dir():
        return
    for path in sorted(bindings_dir.glob("*.qml")):
        lines = path.read_text(encoding="utf-8").splitlines()
        for number, line in enumerate(lines, 1):
            previous = lines[number - 2] if number >= 2 else ""
            guarded = "Cpp_CommercialBuild" in line or "Cpp_CommercialBuild" in previous
            for symbol in COMMERCIAL_SYMBOLS:
                if symbol in line and not guarded:
                    fail(
                        errors,
                        f"{path.name}:{number}: {symbol} reference without a "
                        f"Cpp_CommercialBuild guard on the same or previous line",
                    )


def alias_reference_counts(aliases: dict[str, str]) -> dict[str, int]:
    counts = {alias: 0 for alias in aliases}
    for source_root in SOURCE_ROOTS:
        for path in sorted(source_root.rglob("*")):
            if path.suffix not in SOURCE_EXTS:
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            for alias in counts:
                if alias in text:
                    counts[alias] += text.count(alias)
    return counts


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--alias-report", action="store_true")
    parser.add_argument("--require-no-alias-refs", action="store_true")
    args = parser.parse_args()

    if not ICONS.is_dir():
        sys.exit(f"registry-verify: missing icon tree {ICONS}")
    errors: list[str] = []
    check_tree(errors)
    aliases = check_qrc(errors)
    ids = check_manifests(errors)
    check_layouts(errors, ids)
    check_binding_guards(errors)

    referenced = 0
    if aliases and (args.alias_report or args.require_no_alias_refs):
        counts = alias_reference_counts(aliases)
        live = {a: n for a, n in counts.items() if n}
        referenced = sum(live.values())
        if args.alias_report:
            for alias, count in sorted(live.items(), key=lambda kv: (-kv[1], kv[0])):
                print(f"  {count:4d}  {alias}")
        print(
            f"aliases: {len(aliases)} ({len(live)} still referenced, "
            f"{referenced} total source refs)"
        )
        if args.require_no_alias_refs and live:
            errors.append(f"{len(live)} aliases still referenced by sources")

    for message in errors:
        print(f"FAIL: {message}")
    print(
        f"registry-verify: {'CLEAN' if not errors else f'{len(errors)} violation(s)'}"
    )
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())

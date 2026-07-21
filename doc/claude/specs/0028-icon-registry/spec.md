---
spec: 0028-icon-registry
title: Centralized icon registry (category/id + size)
status: draft        # draft -> approved -> in-progress -> done | shelved
created: 2026-07-21
author: Alex Spataru
---

# Spec 0028 — Centralized icon registry (category/id + size)

> **Phase 1 of 4 — the WHAT and the WHY.** No implementation detail; no file paths, no
> class names, no signal wiring (that is `plan.md`). Gate: do not start `/ss-plan` until
> a human marks this `approved`.

## Problem / Motivation

The application's fixed UI icons are 447 SVG files spread across 17 per-surface folders,
each registered individually in the resource collection and referenced by roughly 800
hardcoded path strings across QML and C++. The same logical icon exists as up to 9 files:
83 files are byte-identical copies of another file (64 duplicate groups — e.g. the window
controls exist twice, the taskbar tool icons exist twice), and a further 66 groups are the
same glyph exported at different Office-pack detail tiers (XS/S/M/L) under folders whose
names encode a surface, not a size. The ~447 files collapse to roughly 261 logical icons.

Because consumers hardcode one specific file, any surface that renders at a different size
than the folder it borrowed from gets the wrong artwork. This is live today: the taskbar
model bakes small-tier workspace/folder icon paths into its node data, and the new command
palette and workspace switcher reuse that model but draw 32 px browse cells — so they
upscale artwork exported for ~16 px display. The size choice is also duplicated ad hoc in
code (a boolean "large" flag on the dashboard widget icon helper, per-surface folder picks
everywhere else). Adding one new icon today means exporting it once per surface folder and
registering every copy.

## Goals

- A single, QML- and C++-friendly way to ask for an icon by **logical id (within a
  category) plus requested display size**, returning something a QML `Image`/`IconImage`
  or a C++ model role can consume directly.
- Surfaces that share a model but render at different sizes (taskbar 16-18 px, start menu,
  workspace switcher and command palette browse cells at 32 px+) each get the artwork tier
  appropriate to their size from the same model data.
- Adding a new icon stays a two-step act: drop the SVG file(s) in the icon tree and
  register them in the resource collection. No C++ table, enum, or QML list to edit.
- The icon tree is consolidated: byte-identical duplicates are gone, same-glyph size
  variants live under one logical id, and the on-disk layout encodes category/id/size
  instead of consuming surface.
- All fixed-UI icon consumers (QML and C++, except the `buttons` set) resolve icons
  through the registry; no hardcoded per-surface icon paths remain on migrated surfaces.

## Non-Goals

- **The `buttons` icon set is exempt.** Its 66 icons keep their current folder, paths, and
  direct references (per maintainer decision).
- **The user-pickable action/workspace icon library is out of scope.** It is a separate
  namespace with its own picker, enumerator, and inline-SVG path; the registry does not
  absorb or re-route it.
- **No tint/metadata contract.** The registry resolves to a URL only; whether and how an
  icon is tinted stays a per-consumer render decision, as today (maintainer decision).
- **No new icon artwork.** Consolidation reuses existing exports; re-exporting missing
  size tiers from the icon pack is a follow-up, not part of this feature.
- **No theme-dependent icons.** A single icon set serves all themes, as today.
- **No behavioral redesign of any surface** — same icons in the same places, at the sizes
  each surface already renders (or intended to render, for the palette/switcher case).

## Requirements

1. **R1** — Both QML and C++ can obtain an icon by *(category, id, requested pixel size)*
   and feed the result directly to the existing rendering primitives (QML image items,
   model string roles) without further string assembly at the call site.
2. **R2** — When a logical icon has multiple artwork tiers, a request is served by the
   tier whose design size is nearest at-or-above the requested size (falling back to the
   largest available); small-size requests never receive a larger tier when a small tier
   exists, and large-size requests never receive an upscaled small tier when a larger one
   exists.
3. **R3** — An icon added by placing file(s) in the icon tree and registering them in the
   resource collection is resolvable immediately, with its size tiers recognized from
   where/how it is placed — no source-code registration step.
4. **R4** — Requesting an unknown category/id fails visibly for the developer (a logged
   warning and a recognizable placeholder or empty result — never a crash) so typos are
   caught during development rather than shipping as silently blank images.
5. **R5** — The consolidated icon tree contains no byte-identical duplicate files, and
   each remaining file is reachable through the registry; same-glyph size variants are
   grouped under one logical id. Genuinely different-detail tiers are kept (maintainer
   decision); merges of "visually similar but not identical" files happen only with
   maintainer visual sign-off.
6. **R6** — The taskbar-model-driven surfaces (taskbar, start menu, workspace switcher,
   command palette) render workspace, folder, tool, and widget icons at their own display
   sizes via the registry: the palette/switcher 32 px+ cells show large-tier artwork while
   the taskbar keeps its small-tier artwork, from the same underlying model.
7. **R7** — All other fixed-UI icon references in QML and C++ (dashboard widget icons,
   project editor tree/model/summary/toolbar, device lists, panes, console, code editor,
   database, licensing, notifications, window controls, toolbar) resolve through the
   registry; grep for the old per-surface path pattern finds no live references outside
   the exempt `buttons` set and the user-pickable action library.
8. **R8** — Visual parity on every migrated surface: each surface shows the same glyph it
   showed before, at the same rendered size, with no new blurring or detail loss. Where
   consolidation replaces a file with another tier of the same glyph, the maintainer
   confirms the result on screen.

## Acceptance Criteria

- [ ] **AC1** — In the running app, the command palette browse grid and workspace switcher
  show crisp large-tier workspace/folder/widget icons at 32 px+, while the taskbar and
  start menu keep their current small icons — confirmed by maintainer observation
  (before/after screenshot comparison).
- [ ] **AC2** — A scripted sweep of the icon tree reports zero byte-identical duplicate
  groups outside the exempt `buttons` set (the current baseline is 64 groups / 83
  redundant files).
- [ ] **AC3** — A grep sweep over QML and C++ sources finds no hardcoded fixed-UI icon
  paths outside the exempt sets (the `buttons` set and the user-pickable action library);
  every migrated reference goes through the registry entry point.
- [ ] **AC4** — Dropping a new SVG into the icon tree and registering it in the resource
  collection (no other edits) makes it resolvable by category/id at multiple requested
  sizes — verified once by the maintainer during review with a scratch icon.
- [ ] **AC5** — Requesting a deliberately wrong id logs a warning and yields the defined
  placeholder/empty result; the app does not crash and no silent blank ships — verified in
  the running app.
- [ ] **AC6** — Full-app visual pass by the maintainer over the migrated surfaces
  (dashboard, project editor, toolbar, panes, dialogs, console, database, licensing,
  notifications, window controls): every icon renders the correct glyph at its prior size.
  Startup and dashboard interaction show no perceptible regression.

## Constraints & Invariants

- **Resource-only registration is the deciding constraint**: the registry derives its
  catalog from what is registered in the resource collection and the icon tree's
  structure; introducing a parallel source-code catalog defeats the feature.
- Icon resolution happens on UI/model-population paths only — it must add nothing to the
  frame hotpath and must not regress the 256 kHz benchmark gate.
- Resolution must be cheap enough for model delegates (called per visible delegate on
  instantiation, potentially hundreds of times on dashboard/palette open) — no per-call
  filesystem scans.
- The taskbar/palette model contract (spec 0027: shared tools model, model-supplied icon
  data, palette free of dashboard-specific symbols) must survive: the palette stays
  reusable and model-driven.
- Existing tinting behavior (per-consumer recolor of monochrome glyphs, full-color glyphs
  untouched) must keep working with registry-resolved icons.
- Works identically in GPL and commercial builds; no new dependencies; no license-gated
  behavior (icons for Pro widgets simply resolve like any other).
- The migration touches many files; it must be split so each surface is independently
  verifiable, and it must not touch the exempt sets or any working-tree file outside the
  migration's own scope.

## Open Questions

- **Q1 — canonical size-tier vocabulary.** The existing exports cluster around four
  Office-pack tiers (roughly 16, 24, 32, 48+ px design sizes) but with inconsistent
  viewBoxes (12/13.5/16/18/22.5/24/30/40/48/60/72). Proposal: normalize to a small fixed
  tier set during consolidation. Decide the exact tier set in `plan.md` from the audit
  data; flag any icon whose only export falls between tiers.
- **Q2 — near-duplicate merge list.** 66 same-glyph groups were identified by name+hash
  analysis; a handful of same-name groups may be genuinely different glyphs (e.g. multiple
  `help` variants). The consolidation needs a per-group audit table with maintainer
  sign-off on merges — produced during planning, reviewed like prior audit passes.
- **Q3 — category taxonomy.** Folders currently encode consuming surface (taskbar, start,
  panes...). After consolidation, categories should encode meaning (e.g. widgets, tools,
  window, devices, editor). The exact category list and the id naming rules (e.g. how the
  current `add-*` composites are treated) need maintainer review in `plan.md`.

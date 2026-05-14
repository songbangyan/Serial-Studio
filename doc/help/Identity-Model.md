# Dataset Identity Model

A dataset in Serial Studio carries three integer identifiers: `index`, `datasetId`, and `uniqueId`. They look interchangeable from the outside but mean very different things, and using the wrong one is the most common reason API calls and transform scripts mysteriously stop working. This page explains each of them in one place, with a copy-pastable rule of thumb at the end.

## The three IDs

A dataset lives inside a group, which lives inside a source. Each level of that hierarchy contributes one number.

### `sourceId`

The index of the source the dataset reads from. `0` for the default (and only) source in single-source projects. With multi-source projects (Pro) sources are numbered `0`, `1`, `2`… in the order they were added.

### `groupId`

The unique identifier of the group inside the project. Group IDs are assigned by the Project Editor and are stable across edits to other groups. Workspace IDs share the same integer space but always start at `1000`, so anything below `1000` is a group, anything `>= 1000` is a workspace.

### `datasetId`

The dataset's slot inside its group. A group with three datasets will have `datasetId = 0, 1, 2`. If you reorder, insert, or delete datasets, `datasetId` is reassigned to match the new array index.

This is the ID you pass to **mutating** API calls:

```jsonc
{
  "method": "project.dataset.update",
  "params": { "groupId": 5, "datasetId": 0, "options": { "title": "Pressure" } }
}
```

### `index`

A separate field unrelated to identity. `index` is the **frame offset**, that is, the position of the dataset's value inside the parsed frame. With a CSV like `25.3,1013.2,42` and three datasets, the first dataset has `index = 1`, the second `index = 2`, the third `index = 3`. (Index `0` is reserved.) Multiple datasets can share the same `index` if you want the same raw value styled in different ways. `index` has no relation to `datasetId` and the two often diverge.

### `uniqueId`

A single integer that identifies a dataset *globally* across the whole project. It's derived from the other three identifiers:

```cpp
uniqueId = sourceId * 1'000'000
         + groupId  * 10'000
         + datasetId;
```

So source 0, group 5, dataset 2 has `uniqueId = 50002`. Source 2, group 17, dataset 0 has `uniqueId = 2'170'000`. The strides (`kSourceIdStride = 1'000'000`, `kGroupIdStride = 10'000`) live in `app/src/DataModel/Frame.h`.

`uniqueId` is what transform scripts and Data Tables use, because they need a single key that's stable across sources. It's what you read from the live data API and what the system data table (`__datasets__`) uses for its `raw:<uid>` and `final:<uid>` registers.

## Where each ID is used

| ID         | Used by                                                                                          |
|------------|--------------------------------------------------------------------------------------------------|
| `sourceId` | Source-scoped API calls (`project.source.update`, `frameparser.compile`, etc.)                   |
| `groupId`  | Group-scoped API calls (`project.group.update`, dataset CRUD addressing)                         |
| `datasetId`| Dataset CRUD addressing (`project.dataset.update`, `project.dataset.setOptions`, `project.dataset.delete`) |
| `index`    | Frame-parser scripts when assigning values from the parsed array (`group.datasets[0].index = 1`) |
| `uniqueId` | Live-data API (`live.dataset.read`), transform scripts (`datasetGetRaw(uid)`, `datasetGetFinal(uid)`), Data Tables (`raw:<uid>`, `final:<uid>`) |

## Why the strides matter

The arithmetic encoding lets any consumer recover the `(source, group, dataset)` triplet from a single integer:

```cpp
sourceId  = uniqueId / 1'000'000;
groupId   = (uniqueId / 10'000) % 100;
datasetId = uniqueId % 10'000;
```

Treat this as an internal detail. Downstream code should not parse `uniqueId` arithmetically, because the strides may grow if a future release needs more groups per source.

## Rule of thumb

The mental shortcut that covers 95% of cases:

> **Mutate by `(groupId, datasetId)`. Read by `uniqueId`. Position by `index`.**

In other words:

- Editing or deleting a dataset? Address it by the group it lives in plus its slot inside that group.
- Reading a live value, looking up a Data Tables register, calling `datasetGetRaw` / `datasetGetFinal` from a transform? Use `uniqueId`.
- Wiring a frame-parser script to pick the right column out of the parsed frame? That's `index`.

## Lifecycle gotchas

- **`datasetId` shifts when you rearrange.** Inserting a dataset at slot 0 renumbers everything that came after. If you cache `datasetId` in your script and then edit the project, refresh it from the API after every mutation.
- **`uniqueId` follows `datasetId`.** Because `uniqueId` is computed from `(sourceId, groupId, datasetId)`, anything that changes `datasetId` also changes `uniqueId`. Don't hard-code `uniqueId` literals in transform scripts that survive across project edits. Resolve them at script start with `tableGet("__datasets__", "raw:" + my_uid)` or by looking up the dataset's `uniqueId` from the project snapshot.
- **`groupId` is stable.** Adding new groups doesn't renumber existing ones. `groupId` is the safest of the three to cache.
- **`index` is yours to set.** Frame parsers control `index` directly. The Project Editor assigns sequential defaults but you can override them.

## Quick reference

```text
Dataset
├─ sourceId   → which source produced the frame
├─ groupId    → which group the dataset belongs to
├─ datasetId  → slot within the group (used for mutations)
├─ index      → which column of the parsed frame to read
└─ uniqueId   → sourceId·1e6 + groupId·1e4 + datasetId  (used for reads)
```

If something is ever ambiguous, default to looking it up fresh from the project snapshot rather than caching it across an edit.

## See also

- [Project Editor](Project-Editor.md): where datasets, groups, and sources are created and where their IDs come from.
- [Frame Parser Scripting](JavaScript-API.md): the parser that picks values out of the frame and assigns them to datasets by `index`.
- [Dataset Value Transforms](Dataset-Transforms.md): per-dataset scripts that read and write registers by `uniqueId`.
- [Data Tables](Data-Tables.md): the system table (`__datasets__`) keyed by `raw:<uniqueId>` and `final:<uniqueId>`.
- [API Reference](API-Reference.md): the JSON-RPC surface that exposes mutating CRUD by `(groupId, datasetId)` and live reads by `uniqueId`.

# Debugging Serial Studio Projects

Five tools cover most "something is wrong" questions.

## meta.snapshot — full status one-shot

Returns project, io, dashboard, console, csv export/player, mqtt,
sessions, mdf4, licensing, notifications status all in one object. Call
this when the user vaguely says "something's not working" — it tells
you immediately whether the connection is up, the project is loaded,
the parser is set, the export is running, etc.

## project.validate — semantic project sanity check

Walks the loaded project and reports issues at three tiers:

- `error` — actually broken: dataset references missing source, group
  references missing source, duplicate dataset index within a group,
  parser fails to compile, FFT enabled with 0 samples
- `warning` — likely-wrong but not crashing: untitled groups/datasets,
  empty groups (except painter), action with no payload
- `info` — design notes: source has no parser, will drop frames silently

`ok: false` only when an error-tier issue is present. Call this after
any non-trivial multi-step build, before declaring success to the user.

## dashboard.tailFrames — the last N samples per dataset

Returns up to 256 recent timestamp+value pairs for each plot-enabled
dataset. Use when the user complains "values look wrong" or "I expected
X but I see Y". You can:

- See exactly what's reaching the dashboard now
- Spot stuck values (always the same number)
- Spot scaling errors (off by 1000x, 0.001x)
- Verify a transform is working

Filter to specific datasets with `{uniqueIds: [...]}`. Default is every
plot-enabled dataset, last 32 samples.

## The dryRun trio

When a script doesn't work as expected, dryRun it BEFORE pushing to the
live project:

- `project.frameParser.dryRun{code, language, sampleFrame}` — compiles
  the parser, runs `parse(sample)` once, returns the rows or
  compile/runtime errors. Fastest iteration loop for parsers.
- `project.dataset.transform.dryRun{code, language, values}` — runs
  `transform(v)` against an array of inputs. Returns per-input outputs.
- `project.painter.dryRun{code}` — verifies compile + that
  `paint(ctx, w, h)` is defined. Doesn't render.

These are the "iterate without committing" lever. Always prefer them
over push-then-revert.

## meta.recentMutations isn't a thing

There's no audit trail of the assistant's own edits in this turn. If the
user asks "what did you change?" — read your own message history; you
made the calls. Don't pretend a recall tool exists.

## Common debugging recipes

### "The dashboard isn't updating"

1. `meta.snapshot{}` — is `io.isConnected: true`? `paused: false`?
2. `dashboard.tailFrames{count: 4}` — are recent samples flowing?
3. If samples are flowing but the dashboard looks stale: ask the user
   to switch workspaces and back. Could be a Qt caching issue.
4. If no samples: parser may be silently rejecting frames. Read the
   parser, eyeball it against a sample frame, dryRun.

### "Values look off"

1. `dashboard.tailFrames` against the affected uids — is the magnitude
   plausible vs. the raw bytes the user expects?
2. If wrong: check transform code via `project.dataset.list` (look for
   `transformCode`); dryRun it with a known input/expected output to
   isolate.
3. If correct in tail but wrong in dashboard: scaling/widget min-max
   issue. Check `wgtMin/wgtMax`, `pltMin/pltMax`, `fftMin/fftMax` on
   the dataset.

### "My script won't compile"

1. dryRun returns compile error. Read it carefully.
2. Common cause for parsers: returning an object instead of array.
3. Common cause for transforms: missing `function transform(value) {}`
   wrapping (the IIFE handles isolation but `transform` must exist).
4. Common cause for painters: function name `draw` instead of `paint`,
   or `bootstrap` (doesn't exist; top-level is bootstrap).

### "Connection drops mid-session"

`meta.snapshot{}` repeatedly — does `io.isConnected` flap? If yes:

- UART: cable, baud rate mismatch, mismatched flow control. Verify
  `io.uart.getConfig` matches the device.
- Network: timeout, NAT teardown. Check
  `io.network.setRemoteAddress` and `setTcpPort` — is the host
  responsive?
- BLE: discovery is stateful and shared — see the BLE skill
  (`meta.loadSkill{name: "can_modbus"}` covers Modbus; BLE is on the
  TODO list).

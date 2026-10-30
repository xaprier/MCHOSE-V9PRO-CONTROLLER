# Patchbay page — design

Date: 2026-10-30. Status: draft for approval.

## Intent

Route audio between programs and devices by drawing cables, like the Graph window of qjackctl /
qpwgraph, inside V9 Pro Control. The motivating case: the sound of a video playing in Zen (browser)
goes into Discord's microphone input so people in the call hear it. This is an extra on top of the
headset features; it works with any PipeWire node, not only the V9 PRO.

## Goals

- New sidebar page **"Patchbay"** (between Equalizer and Device).
- Shows every PipeWire **audio** node as a box: programs (playback/recording streams), devices
  (headset sink/source, other sound cards), and this app's own virtual nodes (EQ, noise suppression,
  mic monitor).
- Each box lists its ports: outputs on the right edge, inputs on the left (port names such as FL/FR,
  `capture_MONO`).
- Existing links are drawn as curved cables, live. Boxes and cables appear/disappear as programs start
  and stop.
- **Connect**: drag from an output port to an input port (either direction of drag is fine).
- **Disconnect**: click a cable to select it, then Delete key or right-click → Disconnect.
- Boxes can be dragged; the canvas can be panned and zoomed (wheel zoom, drag empty space to pan,
  "Fit" button). Box positions are remembered per node identity across restarts.
- **Restore links** (user-facing toggle, **off by default**): when on, links the user created in the
  Patchbay are remembered and recreated whenever both ends exist again (e.g. Zen reopens its stream when
  the video changes). Disconnecting a remembered link in the Patchbay forgets it. A small list shows the
  remembered links with a remove button each.
- Nodes of this app (`v9pro_*`) are visually distinct (accent-tinted header) so it is obvious they belong
  to the EQ / noise suppression / monitor chains.

## Non-goals

- MIDI and video ports/nodes (filtered out).
- Remembering *disconnections* (we do not fight WirePlumber when it links a new stream to the default
  device; if the user wants Zen only in Discord, they disconnect the default cable again).
- Creating virtual devices, loopbacks or mixers from the Patchbay.
- Undo/redo, multiple saved "scenes".
- Replacing pavucontrol-style per-stream volume.

## Anti-goals (would be a failure even if it works)

- Breaking the app's own chains: a stray click must not silently cut the EQ output or noise-suppression
  input. Links between two `v9pro_*` internal ports, and from our filter outputs to their targets, may
  be shown but cannot be deleted without an explicit confirmation.
- Freezing the UI: all PipeWire I/O stays asynchronous (existing rule: no blocking calls on the GUI
  thread).
- Feedback loops created by the auto-restore (e.g. relinking a stream into itself).

## Constraints

- C++20 / Qt 6.8+ / QML, same style as the rest of the app (`Theme`, `Card`, components).
- No new library dependency: talk to PipeWire through its CLI tools, as the app already does —
  `pw-dump --monitor --no-colors` for live graph state, `pw-link <out-port-id> <in-port-id>` to connect,
  `pw-link -d <link-id>` to disconnect.
- Root never required; nothing leaves the machine.

## Design decisions

### Data source

`pw-dump -m -N` prints the full object list once, then JSON arrays of changed objects; a removed object
arrives as `{ "id": N, "info": null }`. A `PipeWireGraph` C++ class (in `src/core`) runs it via
`QProcess`, parses the stream incrementally (top-level JSON arrays, split by bracket depth outside
strings), and keeps maps of nodes, ports and links:

- Node: id, `node.name`, `node.description` / `node.nick` / `application.name` for the label,
  `media.class` (`Audio/Sink`, `Audio/Source`, `Stream/Output/Audio`, `Stream/Input/Audio`,
  `Audio/Duplex`…). Only classes containing `Audio` are kept.
- Port: id, node id, `port.name`, `port.alias`, direction (`in`/`out`), `format.dsp` (skip MIDI),
  `port.monitor` (monitor ports of sinks are kept, they are how "what the headset plays" gets routed).
- Link: id, output port id, input port id, state.

If `pw-dump` exits it is restarted after 2 s (same pattern as `pactl subscribe` in `AudioMixer`).

### Models exposed to QML

- `nodes`: list model (id, label, kind = playback/capture/device/internal, x, y) — positions stored in
  `QSettings` under a stable key (`node.name`, falling back to `application.name` + media class).
- `ports`: list model (id, node id, name, direction, index within node side).
- `links`: list model (id, out port id, in port id, internal flag).
- Invokables: `connectPorts(outId, inId)`, `disconnectLink(linkId)`, `setNodePosition(id, x, y)`,
  `fit()` helpers as needed.

New nodes without a stored position are auto-placed in columns: sources/playback streams left,
filters middle, sinks/recording streams right, stacked top to bottom.

### Restore links

A remembered link is `(output node key, output port name) → (input node key, input port name)`, where
the node key is the same stable key used for positions. Stored in `QSettings`. When the toggle is on,
every graph change checks remembered links whose both ports exist and are not linked, and runs
`pw-link` for them (debounced ~300 ms; at most one attempt per port pair per appearance, so a link the
server refuses does not loop). Rules never touch `v9pro_*`-to-`v9pro_*` pairs.

### QML

`PatchbayPage.qml` with a toolbar (Fit, zoom %, "Restore links" toggle, remembered-links list in a
collapsible card) and a canvas: a `Flickable`/`PinchHandler`-style pannable, zoomable `Item` holding a
`Repeater` of `PatchNode` components and a `Shape` with one cubic `PathCubic` cable per link (and a
"rubber band" cable while dragging). Port hit areas are small rectangles; dragging from one shows the
rubber band and highlights compatible ports (opposite direction, not the same node).

Node colours: playback streams / capture streams / hardware devices / internal (`v9pro_*`) get four
`Theme`-derived tints. Cable colour follows the output node's tint; internal cables are dimmer.

## Left to the builder

- Exact auto-layout spacing, box sizes, zoom limits.
- Whether port labels show `port.name` or a shortened alias (prefer short: `FL`, `FR`, `MONO`).
- Unit tests for the `pw-dump` stream parser and the restore-rule matching (pure functions, as for
  `V9Protocol`), plus a manual test entry in `docs/TESTING.md`.

## Calls made by the assistant (not discussed)

- CLI tools instead of linking libpipewire — consistent with the existing engines, no new dependency.
- Monitor ports of sinks are shown (needed for "send what I hear to Discord").
- Remembered links are only those created inside the Patchbay; disconnecting forgets them.
- Internal-chain cables need a confirmation to delete.
- Page placement: after Equalizer in the sidebar.
- Several streams can share a node key (a browser opens one stream per tab/video, all named "Zen"). A
  remembered link applies to every stream with that key, so all of Zen's audio goes to Discord. Boxes
  with the same key get stacked positions (key + occurrence index) so they do not overlap.

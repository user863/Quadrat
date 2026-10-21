# Quadrat on the web

The same engine the plugin runs, compiled to WebAssembly. `Blocks.cpp`,
`MIBlocks.cpp`, `Presets.cpp` and the vendored Mutable DSP contain no JUCE,
so nothing here is a port of the audio code — `bridge.cpp` only hands the
patch to JavaScript as JSON and lets it push buffers through.

    Source/Blocks.cpp ─┐
    Source/MIBlocks.cpp├─ em++ ─→ quadrat.wasm ─→ AudioWorklet (owns the patch)
    Source/Presets.cpp ┘                              │  snapshot, 20×/second
    web/bridge.cpp ────┘                              ↓
                                              panel.js (canvas)

## Building

GitHub Actions builds it on every push (the `web` job in
`.github/workflows/build.yml`) and offers the page as the `Quadrat-web`
artifact. To build locally you need Emscripten (`em++` on the PATH):

    ./build.sh                # → quadrat.wasm, about 1 MB
    node smoke.mjs            # every preset, checked headless
    python3 -m http.server 8791

and open <http://127.0.0.1:8791/>.

The module is standalone wasm — no emscripten JS glue. It imports three WASI
stubs (`fd_close`, `fd_seek`, `fd_write`, all no-ops) and exports its own
memory, which is why it can be instantiated inside an `AudioWorkletGlobalScope`
where `fetch` and `TextDecoder` do not exist.

## What the page has

A canvas port of the plugin's panel at the same 1120×660 layout, scaled to
the window: the wordmark and about page, the clear / roll / start / stop
caps, the search bar above the block picker, dragging blocks onto the field
(and back to the picker to remove them), moving blocks, drawing and
unplugging cables, knobs, sliders, switches, option strips, block presets,
drawers with depth rings, the sequencer's faders and step switches, scopes
and the clock face, touch links, and tooltips. A preset menu sits in the
header (the plugin leaves that to the host). A–K plays an octave.

## What it does not have yet

The tutorial and welcome card, undo, selection boxes and copy/paste, notes
on the field, the midi block's Logic paste, the in block's audio input, and
saving a patch.

Also worth knowing: `SharedArrayBuffer` is not used, so no COOP/COEP headers
are needed; the snapshot travels by `postMessage` instead.

# ▢quadrat

Quadrat is an free, open-source semi-modular synthesizer, 
with a fun, easy-to-understand interface that allows people to try out modular synthesis. 
Enjoy!


It runs as a **VST3** / **AU** on both Windows and macOS (Apple Silicon/Intel). There is also a Windows Standalone Version and a web demo you can try out here!

<!-- SCREENSHOT: the full panel with a factory preset playing (e.g. "34 Glacier
     Chord"), cables visible, nothing selected. 2240×1320 (2× retina) or 1120×660. -->
<img width="1120" height="660" alt="image" src="https://github.com/user-attachments/assets/82d27ecd-c54e-49a5-bc60-97f5ed916fa8" />

## Download

For Builds see [Releases page](https://github.com/user863/Quadrat/releases).

| Platform | Format | Install location |
| --- | --- | --- |
| Windows | VST3 | `C:\Program Files\Common Files\VST3\` |
| macOS | AU | `~/Library/Audio/Plug-Ins/Components/` |
| macOS | VST3 | `~/Library/Audio/Plug-Ins/VST3/` |

Unzip, copy the `.component` or `.vst3` into your DAW's Plugin Folder, then rescan plug-ins in
your DAW. In Logic Pro, Quadrat is under **AU Instruments → Quadrat →
Quadrat**.

> **The builds are currently unsigned.** On macOS, if the plug-in is blocked by Security, open
> **System Settings → Privacy & Security** and choose **Open Anyway**, or run
> `xattr -dr com.apple.quarantine` on the bundle. Alternatively, you can follow the instructions below to build it yourself from the source.

## Getting started

When you first open Quadrat, there is a quick tutorial you can use to get started. You can find it again at any time by clicking on the
**quadrat** logo.

<!-- SCREENSHOT: the welcome card ("Welcome to Quadrat!") over the panel. -->
![The welcome card](docs/screenshots/welcome.png)

## How patching works

You can connect blocks by placing them next to each other, or by using cables.

Cable colors signify what the signal's purpose is, but you can also connect cables to different inputs.

| Colour | Signal |
| --- | --- |
| green | audio |
| blue | pitch |
| red | gate |
| yellow | modulation |

<!-- SCREENSHOT: close-up of 3–4 touching blocks plus one cable going back to an
     earlier block, so both kinds of connection are visible. -->
<img width="580" height="422" alt="image" src="https://github.com/user-attachments/assets/a1af4efb-ced1-4a07-938a-94da1aa76ce0" />

## Blocks

Search the picker by name or by block function (`filter`, `envelope`,
`delay`, `drums`, `sidechain`, …).

| Group | Blocks |
| --- | --- |
| Sources | `key`, `osc`, `noise`, `in` |
| Voices | `macro`, `timbre`, `drum`, `reson`, `scatter`, `mallet`, `xmod` |
| Shaping | `filter`, `level`, `damage`, `mix`, `lpg`, `ring`, `comp`, `limit` |
| Modulation | `env`, `lfo`, `rand`, `slew`, `quant`, `ctrl` |
| Timing | `clock`, `seq`, `midi`, `delay`, `div`, `logic`, `switch` |
| Generative | `turing`, `euclid`, `chaos`, `ball` |
| Output | `pluck`, `reverb`, `meter`, `out` |

Hovering over a block gives a quick overview of the functionality. Many plugins also have presets of their own.

<!-- SCREENSHOT: the picker with a search typed in (e.g. "filter") and a
     block's tooltip showing. -->
![Searching the picker](docs/screenshots/picker.png)

## Notes and MIDI

- **`key`** plays notes coming from your DAW or MIDI keyboard, with pitch
  bend and sustain.
- **`midi`** loops a phrase in time with the clock. To use it:
  - click its screen and choose a `.mid` file,
  - or drag a `.mid` file onto it (or onto the empty field to create one),
  - or, in Logic Pro, copy notes in the piano roll and press `⌘V` in Quadrat.

Copy and Paste for other DAWs is WIP, but most DAWs keep their Midi Clipboard private, so Midi Drag and Drop is currently the best workaround.

<!-- SCREENSHOT: a midi block holding a loaded phrase, ideally with the drop
     outline visible while a file is dragged over it. -->
![A midi block with a loaded phrase](docs/screenshots/midi.png)

## In your DAW

- Any knob or depth ring you turn becomes a host parameter, named after it
  (for example "osc tune" or "osc shape depth"), so FL Studio's **Last
  tweaked**, automation clips and controller links work on it. Up to 32 at a
  time.
- `ctrl` blocks expose sixteen more controls for host automation, as signals
  you can patch anywhere. In FL Studio they are under **Browse parameters**.
- A session saves the whole patch: blocks, cables, settings and values.
- The `in` block brings audio into the patch: a microphone in the standalone
  app, or a track or side-chain where the host offers one. In FL Studio,
  load Quadrat as an effect in a mixer slot to use it.

## Shortcuts

| Windows | Mac | Action |
| --- | --- | --- |
| `Ctrl+Z` / `Ctrl+Shift+Z` | `⌘Z` / `⇧⌘Z` | undo / redo |
| `Ctrl+A` | `⌘A` | select all |
| `Ctrl+C`, `Ctrl+X`, `Ctrl+V`, `Ctrl+D` | `⌘C`, `⌘X`, `⌘V`, `⌘D` | copy, cut, paste, duplicate |
| `Ctrl+Shift+C` / `Ctrl+Shift+V` | `⇧⌘C` / `⇧⌘V` | copy / paste the whole patch|
| `Delete` or `Backspace` | `Delete` | remove  selection |
| `Shift`-drag knob | `⇧`-drag knob | fine adjustment |
| `Shift`-click a driven control | `⇧`-click a driven control | unplug cable |
| drag on empty space | drag on empty space | box-select |
| scroll, or `Alt`-drag | scroll, or `⌥`-drag | pan |
| `Ctrl` + scroll | `⌘` + scroll, or pinch | zoom |
| `Ctrl+0` | `⌘0` | reset the view |

## Building from source

Requirements: CMake 3.22+, Git, Xcode Command Line Tools on
macOS.

```sh
git clone --recurse-submodules https://github.com/user863/Quadrat.git
cd Quadrat
```

macOS (builds a universal binary and installs the AU and VST3):

```sh
cmake -S . -B build
cmake --build build
```

## Web version

`web/` is a demo of the plugin in WebAssembly, it still lacks certain functionality, 
but you can use it to try out the plugin before downloading:)


<!-- SCREENSHOT: the web version in a browser window. -->
![The web version](docs/screenshots/web.png)

## Contributing

Bug reports and host compatibility reports are very welcome! Please include your
DAW, OS and plug-in format.

## License

Quadrat is free open source software under the GNU General Public License, version 3; see
[`LICENSE`](LICENSE). You may use, study, share and change it. Anything
you distribute that is based on it must be released under the same license,
with its source. It comes with no warranty.

Third-party licenses:

- **STM32F** DSP code in `third_party/mi`, under the MIT license; see `third_party/mi/LICENSE-MIT`
  and `third_party/mi/NOTICE`. Authored by Emilie Gillet (emilie.o.gillet@gmail.com), See https://github.com/pichenettes/eurorack for details.
- **JUCE**, submodule in `third_party/JUCE`, used under the AGPLv3.
- **Inter**, font, under the SIL Open Font License; see
  `Resources/Inter-OFL.txt`.

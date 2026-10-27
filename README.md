# Avel Ess

De-esser in the Avel series, modelled on the workflow of FabFilter Pro-DS. VST3 (macOS, Windows) and Audio Unit (macOS), JUCE 8.

## Engine
- **Two detection modes.**
  - *Single Vocal* compares the energy in the detection band with the whole signal, so bright vowels and breathy overtones don't trigger it. Only real sibilance does.
  - *Allround* reacts to the band level alone. Use it for mixes, drums, acoustic guitars, synths and reverb returns.
- **Wide band / Split band.**
  - *Wide band* turns the whole signal down on each "s", like a classic de-esser.
  - *Split band* turns down only the band. The band is a linear-phase FIR, so the result is bit-identical to the input whenever there is no reduction, and nothing below the band changes. The test measures 0.000 dB change at 180 Hz while 12 dB is taken off the "s".
- **Lookahead** 0–15 ms, so the reduction is already in place when the "s" starts. Latency equals the lookahead plus 2.6 ms of FIR centre, and it is reported to the host.
- **Detection band** from 1 to 16 kHz, with an optional upper edge from 2 to 20 kHz (*Open* means no upper limit).
- **Threshold, Range** (maximum reduction, 0–24 dB), **Release** 5–250 ms.
- **Channels:** Stereo with adjustable Stereo Link, Mid only, or Side only.
- **Audition** plays the band being processed.
- **Input / Output** gain.
- CPU use is about 3.5 % of one core at 48 kHz stereo in split band mode.

## UI
- **History.**
  - Input level (grey), detection level (teal, bright above the threshold) and gain reduction (red, hanging from the top) scroll by over about 3 seconds.
  - Drag the threshold line to set it, Shift for fine moves, double-click to reset.
  - *Max reduction* shows the peak value. Click it to reset.
- **Band view.**
  - The input spectrum with the detection band highlighted. The band turns red while it is being reduced.
  - Drag the L or H edge, or drag the band itself to move both edges. Shift for fine moves, double-click an edge to reset it.
- **Dials:** drag, Shift for fine moves, wheel, double-click to reset, click the value to type a number (`5.5k`, `-30`, `12 ms`).
- **Presets:** 16 factory presets, plus user presets in `~/Documents/Avel/Ess/Presets/*.avelpreset`.
- A/B with copy, and undo/redo for every change.
- The window is resizable, and its size is saved with the session.

## Install
- **macOS:** `AvelEss-<version>-macOS.pkg`. Double-click it, then Continue and Install. This installs VST3 and AU into `/Library/Audio/Plug-Ins`. If macOS refuses to open the file: System Settings → Privacy & Security → Open Anyway.
- **Windows:** `AvelEss-<version>-Windows-Setup.exe`. This installs VST3 into `C:\Program Files\Common Files\VST3`. If you see "Windows protected your PC": More info → Run anyway.

Both installers are built by GitHub Actions (`.github/workflows/build.yml`) on every push. Each build validates the plugin with auval and pluginval, test-installs it and publishes it on the Releases page.

## Build
- **macOS:** `bash build_mac.sh` builds the plugin and installs it into `~/Library/Audio/Plug-Ins`. You only need the Xcode Command Line Tools: CMake and JUCE are downloaded automatically. `bash make_installer_mac.sh` writes the `.pkg` to `dist/`.
- **Windows:** `cmake -S . -B build`, then `cmake --build build --config Release --target AvelEss_VST3`, then run Inno Setup on `packaging/windows/AvelEss.iss`.
- `-DJUCE_DIR=/path/to/JUCE` uses a local JUCE checkout instead of downloading it.

## Tests
- `Tests/EngineTest.cpp` is a standalone DSP test. It checks transparency, latency, Range accuracy, vowel rejection in Single Vocal, split-band isolation, Mid mode, Stereo Link, lookahead timing, stability and CPU use:
  `g++ -O2 -std=c++17 -ISource Tests/EngineTest.cpp -o engine_test && ./engine_test`
- `cmake -S . -B build -DAVEL_BUILD_TESTS=ON` adds `AvelEssTest`, the offline render test. It feeds a synthetic vocal through the plugin, writes UI screenshots to `shots/`, and checks presets, the state round trip, A/B and undo.
- pluginval passes at strictness 10.

`Source/UI/Theme.h` follows the Avel design system. Keep it in sync with the other Avel products.

Plugin IDs: manufacturer `Avel`, plugin `AvEs`, bundle `com.avel.ess`. Installer packages: `com.avel.ess.vst3` / `com.avel.ess.au`.

## Licenses
Built on JUCE 8 (AGPLv3 / commercial JUCE licence). The Inter font is distributed under the SIL Open Font License (`Resources/Fonts`).

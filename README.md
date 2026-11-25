# GOD ATOM

GOD ATOM is a VST3 mix-bus / master-chain processor. It sits on a master or a bus in FL Studio and gives you tone, glue, saturation, width and a safe limiter from a handful of big controls.

It is original software: original DSP, original artwork, original name. The layout follows the same left-to-right job as a typical one-knob master chain (input, EQ, amount, gain reduction, limiter, output) so public tutorials for that kind of plugin are easy to follow.

## Controls

- **Input** — level into the chain. Aim for the green brackets on the left meters.
- **High / Mid / Low** — a simple pre-EQ, ±6 dB. These stay active when Amount is at 0.
- **Amount** — 0–200%. This is the character: a mid/side house curve, mid-keyed multiband glue, a high-side tilt, and odd-harmonic saturation. 100% is the starting point.
- **Limiter** — loudness. Default is +5 dB of input gain. The ceiling is −0.1 dBFS (true-peak-ish). Raising Output cannot emit overs.
- **Output** — level after the limiter.
- **Sidechain INT / EXT** — the multiband detector can key off the mix or an external input. The limiter always keys off the main signal.
- **Settings** — oversampling (1× / 2× / 4× / 8×), RMS or short-term LUFS meters, animation on/off, auto-gain compare.

Factory presets: Default, Indie, Ambient, Late Night, Folk, Glue Only, Wide Air, Quiet Mix.

**Save** writes a named user preset to `~/Library/Audio/Presets/GOD ATOM` (an XML snapshot of the current parameters) and adds it to the preset menu. The same folder is scanned on launch, so those presets come back in a new session.


## Latency

At 1× oversampling the plugin reports a 64-sample lookahead (about 1.33 ms at 48 kHz), including when the limiter is off, so the delay does not jump when you click the limiter. Higher oversampling adds a small, reported filter delay.

## Build

VST3, JUCE 8.0.4, CMake, C++20. See `TESTING.md`. The Mac universal zip is produced by GitHub Actions on `macos-15-intel` once the repo is public. Until then, build and test on your machine:

```
cmake -S . -B build -DGODATOM_ENGINE_ONLY=ON
cmake --build build
./build/godatom_engine_tests
```

## License

MIT. See LICENSE.

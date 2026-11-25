# Testing GOD ATOM

## Engine tests (no JUCE window)

```
cmake -S . -B build-engine -G Ninja -DGODATOM_ENGINE_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-engine
./build-engine/godatom_engine_tests
```

These cover bypass/Amount-0 bit-exact null, NaNs, latency vs a measured impulse at 1×/2×/4×/8×, EQ vs the RBJ cookbook, the documented Atom curve, the textbook soft-knee compressor, mid-only detection, the −0.1 dBFS ceiling and a true-peak check, oversampling aliasing, LUFS vs ITU-R BS.1770-4, every control changing the sound, distinct presets, and a CPU realtime ratio.

## Plugin tests (JUCE processor + editor)

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target godatom_plugin_tests GodAtom_VST3
./build/godatom_plugin_tests_artefacts/Release/godatom_plugin_tests
```

These take screenshots (including default layout at 75%, 100% and 200% UI scale) and write level-matched WAV demos into `diag/`. They also save, list and reload a named user preset under `~/Library/Audio/Presets/GOD ATOM`, then delete the test file.


## pluginval

On the Mac CI zip (public repo only), pluginval runs at strictness 10 and must pass.

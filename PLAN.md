# GOD ATOM — all-in-one mix-bus / master chain for FL Studio

Status: **build spec.** Original code, original art, original name. Do not install on the owner's Mac until they say "install for me." Do not make the repo public until they explicitly say so for *this* repo. The owner's name must not appear in any repo file, comment, preset, credit, or install note.

**GOD ATOM** is a VST3 audio effect you put on the master (or a mix bus) in FL Studio. It is a one-stop chain: tone/EQ, glue compression, saturation, stereo width, low-end control, and a final limiter, driven by a few big controls. It is inspired by the *job* and the *layout* of Cradle's The God Particle, so public God Particle tutorials can be followed on this plugin. It is **not a copy**. The processing, the name, the logo, the fonts, the knob artwork, and the particle graphics are original. No Cradle graphics, names, or preset wording are reused.

A public repo with a look that follows The God Particle closely has some trademark/trade-dress risk; the product name is GOD ATOM and the repo is koobacorp/godatom (private unless the owner later OKs a public copy).

Primary use: the FL Studio master channel, for alternative, indie, ambient, electronic-folk, and club-adjacent tracks. It also works on buses. No machine learning and no paid compute.

---

## 1. What you do with it

1. Put GOD ATOM on the master (or a bus) **at the start of the mix**, and mix into it. This is how The God Particle is meant to be used ([Cradle, “How do I use The God Particle?”](https://support.cradle.app/hc/en-us/articles/6423895706004-How-do-I-use-The-God-Particle); [MusicTech review, June 2022](https://musictech.com/reviews/plug-ins/cradle-the-god-particle-review-a-grammy-winning-mix-bus-chain-in-a-simple-package/)).
2. Set **Input** so the left-hand meters land in the green target brackets.
3. Leave **Amount** at 100% unless it is too much; turn down, or pick a softer preset (Ambient, Glue Only, Quiet Mix). Amount goes from 0% to 200%, same as The God Particle ([Cradle GUI Overview](https://support.cradle.app/hc/en-us/articles/4411750433044-GUI-Overview)).
4. Use the **High / Mid / Low** EQ knobs if a band is missing the green gain-reduction targets, or to add air, body, or low warmth. These are ±6 dB ([BchillMusic interface notes](https://bchillmusic.com/the-god-particle-plugin-review-mixing-tutorial/)).
5. When the mix is ready, use the **Limiter** knob for loudness. The default is +5 dB of limiter input gain, matching the published God Particle default (Cradle team, quoted in [Everything Recording](https://everythingrecording.com/god-complex-cradles-the-god-particle-review/)). Our ceiling is **−0.1 dBFS** (true-peak-ish), not 0 dBFS.
6. Compare with the in-plugin **Bypass** or **A/B**. Turn **Auto-gain compare** on (Settings) if you want to hear tone, not loudness.

It should already sound useful at the Default preset. Everything else is there when you need it.

---

## 2. What The God Particle actually does (public facts)

Cradle does not publish the exact inner coefficients ([Cradle, “what's going on inside”](https://support.cradle.app/hc/en-us/articles/6424232645908-Can-you-tell-me-more-about-what-s-going-on-inside-The-God-Particle)). Public docs and reviews agree on the job and the controls:

- All-in-one mix-bus processor: EQ, multiband compression, saturation/excitation, widening, adaptive limiter ([MusicTech](https://musictech.com/reviews/plug-ins/cradle-the-god-particle-review-a-grammy-winning-mix-bus-chain-in-a-simple-package/); [Cradle product page](https://cradle.app/products/the-god-particle)).
- Controls: Input, 3-band EQ (high/mid/low), Amount 0–200% (default 100%), multiband gain-reduction meters with green targets, INT/EXT sidechain, Limiter, Output, plus peak + RMS (or short-term LUFS) meters. Peak turns red over 0 dB. Clicking a reading resets it ([Cradle GUI Overview](https://support.cradle.app/hc/en-us/articles/4411750433044-GUI-Overview); LUFS and oversampling added in v1.2.2, [release notes](https://support.cradle.app/hc/en-us/articles/6423691528468-Updates-and-Release-notes) and [Oversampling](https://support.cradle.app/hc/en-us/articles/15947880896532-Oversampling) / [LUFS Metering](https://support.cradle.app/hc/en-us/articles/15947889022484-LUFS-Metering)).
- EQ and Limiter are independent of Amount. Amount scales the “character” section only ([VI-Control analysis of v1.2.4.0](https://vi-control.net/community/threads/jaycen-joshuas-the-god-particle-plug-in.125673/page-16)).
- Published signal path from that same public analysis: In → 3-band EQ → house-curve EQ → multiband comp → side HF tilt → saturation → limiter → Out. Multiband detection is mid-only and stereo-linked (side-only material does not trigger it). Saturation is symmetric odd-harmonic, split into three bands. House curve at 100% boosts lows ~3 dB, mids ~3.5 dB, and highs more on the side than the mid. Crossover points have been estimated around 500 Hz and 10 kHz ([Everything Recording](https://everythingrecording.com/god-complex-cradles-the-god-particle-review/)).
- One factory preset (the published “default”). Users can save their own ([Cradle presets FAQ](https://support.cradle.app/hc/en-us/articles/6424187550868-Are-there-any-presets-included-in-The-God-Particle)).
- What people like: fast, glued, loud, low-effort on the right material; can replace a long chain ([MusicTech](https://musictech.com/reviews/plug-ins/cradle-the-god-particle-review-a-grammy-winning-mix-bus-chain-in-a-simple-package/); [Electronic Production](https://www.electronicproduction.co.uk/post/the-god-particle-plugin-review-jaycen-joshua-s-mix-bus-magic-sauce)).
- What people complain about: not versatile; can sound hyped, dull, or too wide; no (original) LUFS meter; hidden processing; default limiter is aggressive; heavy CPU in FL Studio; iLok-style Hub auth; GR meters are misleading when Amount is not 100%; internal resample to 44.1 kHz in the analysed version ([MusicTech](https://musictech.com/reviews/plug-ins/cradle-the-god-particle-review-a-grammy-winning-mix-bus-chain-in-a-simple-package/); [Everything Recording](https://everythingrecording.com/god-complex-cradles-the-god-particle-review/); [Image-Line forum CPU reports](https://forum.image-line.com/viewtopic.php?t=288838); [VI-Control](https://vi-control.net/community/threads/jaycen-joshuas-the-god-particle-plug-in.125673/page-16)). Cradle itself notes the default targets may not fit a pastoral folk track ([“How do I use”](https://support.cradle.app/hc/en-us/articles/6423895706004-How-do-I-use-The-God-Particle)).

GOD ATOM covers the same job, in the same control order, with original DSP. It will not sound identical, and we do not claim it does.

---

## 3. Controls, positions, and ranges (tutorial map)

Layout follows the public God Particle GUI (official product shots and in-use screenshots such as [MusicTech](https://musictech.com/wp-content/uploads/2022/05/Cradle-The-God-Particle@2000x1500.jpg) and the in-use capture used in [Everything Recording](https://everythingrecording.com/god-complex-cradles-the-god-particle-review/)): a slim top bar, a black “glass” display, and a dark metal chassis with a speaker grille. Default size is about 1040×430 at 100% UI scale, resizable.

### Top bar (left to right)

| Control | Position | Range / values | What it does |
|---|---|---|---|
| **Preset** | Far left, dropdown | Factory + User | Load a factory preset or a saved user preset. Disk icon saves the current settings under User. Factory includes Default plus genre presets (see §7). |
| **A / B** | Next to Preset | Two slots + copy | Two setting slots so you can flip between them. |
| **Sidechain INT / EXT** | Mid-right | INT (default) or EXT | INT: the mix itself drives the multiband detector. EXT: an external key (the plugin’s sidechain input) drives it. The limiter always keys off the main signal, same as Cradle’s stem notes ([stem export FAQ](https://support.cradle.app/hc/en-us/articles/12637616897940-How-do-I-export-stems-using-The-God-Particle)). |
| **UI Scale** | Right | 75, 100, 125, 150, 200% | Scales the window. Dragging the window corner also scales it. |
| **Settings** | Far right | Page | Oversampling 1× / 2× / 4× / 8×; meter mode RMS+Peak or Short-term LUFS+Peak; animation on/off; auto-gain compare on/off. |

### Display (left to right)

| Control | Position | Range | What it does |
|---|---|---|---|
| **Input meters** | Far left, two vertical bars (L/R) | Peak and RMS (or short-term LUFS) | Top pair: peak, turns red above 0 dBFS. Lower pair: RMS or LUFS. Click a reading to reset. Green square-bracket **targets** mark the input sweet spot (a guide, not a law). |
| **EQ power** | Top of the EQ column | On (default) / off | Bypasses the three-band pre-EQ only. |
| **High / Mid / Low** | Three stacked silver knobs under EQ power | −6 to +6 dB, default 0, 12 o’clock | Pre-character EQ. High is air (high shelf ~10 kHz). Mid is body (wide bell ~640 Hz, Q 0.5). Low is warmth (low shelf ~200 Hz). These stay active when Amount is 0. |
| **Amount knob** | Centre-left of the galaxy | 0–200%, default 100%, 12 o’clock = 100% | How much character is applied (house curve, multiband glue, side high tilt, saturation). 0–100% is a dry/wet blend of the character section. 100–200% is full wet, with a stronger house curve and more drive. |
| **Amount power** | Under the “amount / 100%” readout | On (default) / off | Bypasses the whole character section (same idea as Amount = 0, but one click). EQ and Limiter stay as they are. |
| **Gain reduction** | Right of Amount, stacked High / Mid / Low | Meters in dB | Honest gain reduction for each multiband band, plus green target brackets as a mixing guide. Unlike the analysed God Particle, these numbers follow the real reduction at any Amount, not only at 100%. |
| **Limiter power** | Top of the limiter column | On (default) / off | Bypasses limiter gain reduction (the lookahead delay stays in the path so reported latency does not jump). |
| **Limiter meters** | Under the limiter power | Two GR readings | How much the limiter is pulling down, L/R. |
| **Output meters** | Far right, two vertical bars (L/R) | Peak and RMS (or LUFS) | Same style as input. No input-style target brackets. Peak turns red above 0 dBFS (the processor itself ceilings at −0.1). Click to reset. |

### Chassis (bottom row, left to right)

| Control | Position | Range | What it does |
|---|---|---|---|
| **Input** | Large silver knob, far left | −12 to +12 dB, default 0 | Level into the chain. Use it to hit the green input target. |
| **Name plate** | Under the grille | — | Widely spaced small caps: **GOD ATOM**. Our lettering, in the same place tutorials put the product name. |
| **Limiter** | Silver knob, right of the grille | 0 to +12 dB, default +5 | Limiter input gain. Higher is louder. Ceiling is −0.1 dBFS true-peak-ish. |
| **Output** | Silver knob, far right | −12 to +12 dB, default 0 | Level after the limiter. A safety ceiling still holds the plugin output to −0.1 dBFS so raising Output cannot emit overs. |

In-plugin **Bypass** (host bypass, or an editor bypass if shown) is a true delayed bypass: bit-exact copy of the input, through the same latency delay as the active path, so it stays aligned.

---

## 4. Colour palette

Sampled from a public in-use screenshot of The God Particle and redrawn by us (no copied images, logos, or font files). Chassis labels are a little lighter than the original so the UI meets the high-contrast brief without moving any control.

| Role | Hex | Notes |
|---|---|---|
| Display / glass | `#010001` | Near black |
| Chassis | `#433D3E` | Dark warm grey |
| Grille texture | `#434041`–`#676465` | Horizontal slats, drawn, not a bitmap |
| Chassis labels | `#C8C4C5` | Input, EQ, Gain reduction, Limiter, Output |
| Accent mint (power on, numbers, “amount”, band labels) | `#8CD4BE` | Median sampled mint |
| Meter fill / GR fill / target brackets | `#1A7244` | Forest green |
| Meter track | `#07130D` | Very dark green-black |
| Knob face | `#B8B5B7` | Brushed silver, radial |
| Knob highlight | `#ECE9EC` | Specular edge |
| Amount percentage | `#FFFFFF` | Large, centred in the capsule |
| Clip / over | `#E23B3B` | Peak readout above 0 dBFS |
| Galaxy particles | white, gold, teal, magenta | Our own animation around the Amount knob; can be turned off in Settings |

Fonts are system / JUCE defaults, not a copied typeface. Contrast of mint `#8CD4BE` on `#010001` is well above 7:1.

---

## 5. Signal chain (our original path)

```
Input gain
  → 3-band pre-EQ (if on; skipped when all bands are 0 dB)
  → Character (if on and Amount > 0):
        mid/side
        house / “Atom” curve (Amount-scaled, mid and side have different highs)
        3-band crossover (~500 Hz and 10 kHz)
        multiband compressor (detect mid, or external mid; apply linked GR to mid and side)
        side high-frequency tilt
        mid/side decode
        3-band odd-harmonic saturation
        dry/wet by Amount (0–100%); extra curve and drive (100–200%)
  → Lookahead limiter (if on)
  → Output gain
  → Safety ceiling at −0.1 dBFS (true-peak-ish)
```

Processing runs at the **host sample rate**. We do not resample everything to 44.1 kHz. Oversampling (1× default, 2× / 4× / 8×) is for the saturator and limiter, to cut aliasing.

### Atom curve (documented targets we test against)

Inspired by the public house-curve writeup, not a copy of hidden coefficients.

At Amount 100%: low shelf 150 Hz, mid and side +3.0 dB; mid bell ~800 Hz, +3.5 dB; high shelf ~10 kHz, mid +1.0 dB, side +4.0 dB.

At Amount 200%: low +4.0 dB; mid bell mid +5.8 / side +7.7 dB; high mid +3.0 / side +7.0 dB.

0–100% and 100–200% interpolate those points.

### Multiband

Linkwitz–Riley crossovers at 500 Hz and 10 kHz. Detector is the mid channel (or the external sidechain mid). Soft-knee, moderate ratio, wide knee. Makeup so 100% still feels level, with the Atom curve doing the tonal lift. Side-only audio must not trigger reduction. Meters show the real GR at the current Amount.

### Saturation

Symmetric (odd harmonics only), three bands, drive follows Amount. 0–100% is mostly the dry/wet of the whole character section.

### Limiter

Adaptive release, 64-sample lookahead at 1× (~1.33 ms at 48 kHz), true-peak style detection (4× interpolator). Ceiling **−0.1 dBFS**. Output gain cannot break that ceiling.

---

## 6. Latency, loudness, and hard rules

- **Report latency correctly.** At 1×, latency is the limiter lookahead (64 samples), even if the limiter is off, so it does not jump when you click the limiter power. 2× / 4× / 8× add the oversampler delay. `getLatencySamples()` must equal a measured impulse delay in that configuration.
- **Ceiling −0.1 dBFS**, sample peak and true-peak-ish. No overs out of the plugin.
- **Bypass and Amount 0 (with EQ flat or off, limiter powered off, Input and Output at 0) are bit-exact** with each other, through the delay line. No NaNs, no infinities, denormals flushed.
- **A/B auto-gain** (Settings, on by default for compare) level-matches short-term LUFS so a compare is about tone, not loudness. It can be turned off to hear the raw loudness change, which is what many God Particle tutorials demonstrate with bypass.
- Low CPU: no Hub, no heavy animation if it is turned off, no forced 44.1 kHz resample. Aim to stay comfortable on the master of a full FL session at 1×.

---

## 7. Presets

Factory (our names, not theirs):

- **Default** — Amount 100%, EQ 0, Limiter +5 dB, all sections on. The tutorial starting point.
- **Indie** — a little less limiter, slightly softer highs.
- **Ambient** — lower Amount, less width, gentler limiter.
- **Late Night** — more punch and loudness, for electronic / club-adjacent tracks.
- **Folk** — restrained Amount, modest limiter, less high-side tilt.
- **Glue Only** — moderate Amount, limiter off (or very low), for a bus.
- **Wide Air** — more high-side width and a touch of High EQ.
- **Quiet Mix** — Amount and limiter pulled back, for checking a mix without smash.

Users can save their own under User, and return to Default from the factory list.

---

## 8. Look and feel (original art)

Same *positions*, *control names*, and *colour roles* as The God Particle, so a tutorial’s “turn Amount”, “hit the mid target”, “limiter on the right” still work. Every pixel is drawn by us in JUCE: silver knobs, mint power pills, green meters and brackets, a slatted grille, and a particle field around Amount. No third-party artwork, no copied logo, no copied font file.

The window is resizable (corner drag and UI Scale). High contrast on the display; chassis labels are light grey on dark metal.

---

## 9. Test plan

Engine tests (no window) and plugin tests, plus screenshots and level-matched audio demos in `diag/`. Tests must fail if a rule is broken.

- **Bypass / Amount 0 null:** with EQ off or at 0 dB, character off or Amount 0, limiter off, Input and Output at 0, output equals the delayed input bit-exactly. In-plugin bypass matches that path bit-exactly.
- **No NaNs:** harsh inputs (denormals, ±1, leftovers, DC, full-scale noise, empty buffers) stay finite.
- **Latency report:** measured impulse delay equals `getLatencySamples()` at 1×, 2×, 4×, and 8×, limiter on or off.
- **EQ vs textbook:** low-shelf, mid-bell (Q 0.5), high-shelf magnitude at several frequencies matches the RBJ cookbook within a tight dB tolerance, at 0 / ±3 / ±6 dB. At 0 dB the EQ path is skipped so the null test stays bit-exact.
- **Atom curve vs documented targets:** frequency response at Amount 100% and 200% (character on, compressor held out of the way with a low-level sweep or with thresholds open) matches §5, mid and side separately.
- **Compressor gain curve:** static levels in one band follow a textbook soft-knee curve (ratio, threshold, knee) within tolerance.
- **Mid-only detection:** a side-only signal does not produce mid-band GR; adding mid starts GR.
- **Limiter ceiling:** sample peak ≤ 10^(−0.1/20). A 4×-interpolated true-peak check stays at or under −0.1 dBTP on pathological transients, within a small allowed epsilon.
- **Oversampling aliasing:** a high sine (e.g. 18 kHz at 44.1/48 kHz) into a driven saturator produces less aliased energy at 2× than at 1×, and less again at 4× / 8×.
- **LUFS meter:** short-term meter at 48 kHz is checked against ITU-R BS.1770-4 K-weighting (official 48 kHz coefficients in the test, not copied out of the engine). +6 dB in equals +6 LU. Absolute level of a 997 Hz stereo sine is within a tight tolerance of the independent reference.
- **Every control changes the sound:** each knob and switch, moved off its default, changes the output by more than a small epsilon on a mixed test signal (power-off switches also change it, or truly bypass that section).
- **Presets are distinct:** the same mixed signal through each factory preset produces a different fingerprint (RMS and/or spectrum) than Default, except where a preset is *meant* to be a subtle variant — still measurable.
- **Safety ceiling after Output:** raising Output cannot emit a sample above the ceiling.
- **CPU:** a 10-second 48 kHz stereo buffer at 1× processes many times faster than real time on the build machine; the ratio is printed.
- **pluginval strictness 10** on the Mac CI zip, once a public repo exists. Until then, Linux/box plugin tests and engine tests are the gate.
- **UI shots:** default, Amount 0, Amount 200%, a couple of presets, settings if reachable.

---

## 10. Honest limits versus The God Particle

- It will not sound the same. Their character section is unpublished. Ours is original, tuned for the genres above, and tested against *our* documented curves.
- We will not resample the whole plugin to 44.1 kHz; we will not ship a Hub or an online license; our GR meters are honest at every Amount; our ceiling is −0.1 dBFS, not a 0 dBFS wall.
- A one-knob-style chain still has a point of view. On some tracks it will be too much, too wide, or not the right tone. That is what Amount, the EQ, the presets, and Bypass are for.
- This is not a full mastering suite (no mid/side EQ editor, no custom crossovers, no loudness-target officializer). You already have Maximus and Fruity Limiter; GOD ATOM earns its place by being simpler to use as a one-stop master chain, not by exposing more bands.
- Nothing is verified in FL on the owner’s Mac until they try it.

---

## 11. Build and ship

- Format: **VST3 only**, stereo effect, optional stereo sidechain input. Manufacturer koobacorp, product **GOD ATOM**, bundle id `com.koobacorp.godatom`.
- Stack: JUCE 8.0.4 + CMake, C++20, original code. Universal macOS (x86_64 + arm64) via GitHub Actions `macos-15-intel` is the intended release path, but that runner is free only on a **public** repo. Until the owner OKs public, the repo stays **private**, box/Linux tests are the gate, and no Mac CI zip is produced.
- pluginval 10 belongs in the public-repo workflow, required (not `continue-on-error`).
- Ad-hoc codesign on the Mac zip, same as the other plugins. Zip also belongs on the Desktop REP PLUG folder **at install time**, and only when asked.
- No paid compute.

---

## 12. What “go” means

Build it fully, test it fully, take screenshots and level-matched demos, and put the source in a **private** `koobacorp/godatom` repo with a scrubbed history. Do not make it public. Do not install it on the Mac. Report the tests, the paths, the latency, and anything that is still rough. Then wait.

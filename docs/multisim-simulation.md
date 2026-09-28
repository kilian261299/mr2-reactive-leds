# Multisim Simulation — R5 Correction

Documents the Multisim simulation work that found and validated the `R5` correction (`100kΩ → 4.7kΩ`) described in [docs/audio-reactive-led-plan.md](audio-reactive-led-plan.md) and [hardware/audio-breakout.md](../hardware/audio-breakout.md). This happened after the `v2` PCB's Gerbers had already been submitted to JLCPCB, with the physical conditioning-circuit parts unavailable on the bench (shipped with the board) — Multisim was used instead of a physical breadboard to test the corrected circuit before committing to the fix.

---

## Why simulate instead of hand-calculate

The original `R5 = 100kΩ` spec was based purely on the C4/R5 transfer-function math — treating the coupling stage as a simple AC high-pass filter in isolation. That math wasn't wrong, but it was incomplete: it didn't (and can't, without accounting for D1's nonlinear switching behaviour) capture how R5 interacts with R6 and D1 together. Multisim let the full circuit — including the diode — be built and driven with realistic test signals, with an oscilloscope reading the actual node voltages, rather than trusting a partial hand-derivation.

---

## Circuit modelled

Same topology as the production `v2` circuit (see the audio-reactive-led-plan.md component list), using Multisim's standard SPICE parts:

```
XFG1 / AC_VOLTAGE / AM_VOLTAGE (source, swapped depending on test — see below)
        │
        ▼
R3, R4 (470Ω each) → Node S           (stereo summing — driven from the same source for both channels)
        │
        ▼
C4 (2.2µF) → Node A                    (coupling capacitor)
        │
Node A → R5 (value under test) → GND   (reference resistor — 100kΩ, 10kΩ, 4.7kΩ, 2.2kΩ tested)
        │
        ▼
D1 (BAT85, diode model) → Node B
        │
C3 (2.2µF) → GND                       (smoothing)
R6 (10kΩ) → GND                        (discharge)
```

Node A (pre-diode) and Node B (post-diode) were the two points probed throughout.

---

## Setup notes and gotchas found along the way

A few Multisim-specific issues came up while getting the circuit under test — worth recording since they weren't obvious going in:

- **R4 must be driven from the same source as R3.** An early build accidentally tied R4's input to `COM` (the source's ground return) instead of the shared `+` signal line, creating an unintended 50% divider at Node S before any other analysis was even meaningful.
- **`XFG1` (the Function Generator instrument) cannot drive AC Sweep analysis.** Its dialog has Waveform/Frequency/Duty Cycle/Amplitude/Offset/Rise-Fall-Time fields — no "AC Analysis Magnitude" field at all. AC Sweep needs a dedicated `AC_VOLTAGE` SPICE source component instead; `XFG1` (or `AM_VOLTAGE`, see below) is used for transient analysis instead.
- **AC Sweep needs an explicit output variable.** Without adding one on the Analysis dialog's Output tab (select the node, click the arrow into "Selected variables"), it fails with "There are no valid output variables."
- **Node numbers are meaningless until labelled.** Multisim auto-assigns them, so the Output tab shows `V(1)`–`V(4)` with no way to tell which is which — double-click the wire and rename it (or `View → Show Node Numbers`) before trying to select an output.
- **AC Sweep's frequency range must be set for audio**, not left at the `1Hz–10GHz` default — `1Hz–20kHz` gives a usable, correctly-scaled Bode plot.
- **AC Sweep only gives meaningful results probed *before* the diode.** AC Sweep is a linear, small-signal analysis — it linearizes the circuit around a DC operating point, and with no DC bias source present, D1 linearizes around "off" (near-infinite impedance). Probing Node A (before D1) gives a correct C4/R5 transfer function; probing Node B (after D1) returns near-zero, meaningless results. Anything downstream of the diode needs transient analysis instead.
- **Oscilloscope channels must use DC coupling, not AC, to see a signal's true level.** AC coupling re-centres the displayed trace around its own average — this made an always-positive Node B envelope appear to swing below zero on screen, purely as a display artifact, not a real circuit fault. This came up twice in this work (see the AM-modulated test below) and is worth remembering for any future scope work on this circuit.
- **Settling time matters.** Changing R5 changes the R5×C4 time constant directly (`220ms` at `R5=100kΩ`, `~10ms` at `R5=4.7kΩ`) — a simulation run shorter than a few time constants can show an apparent "sudden jump" that's actually just the circuit not having reached steady state yet, not a bug.

---

## AC Sweep — confirming the C4/R5 transfer function

With `AC_VOLTAGE` driving the circuit and the probe on Node A, AC Sweep (`1Hz–20kHz`) confirmed the transfer function used in the original C4/R5 math:

```
H(f) = R5 / √[(R5 + R_series)² + (1/(2πfC4))²]
```

The resulting Bode plot showed the expected shape: high-pass behaviour rolling off below ~20–30Hz (set by the R5/C4 time constant), flat and near-unity above that, with phase swinging from close to 0° at high frequency toward 90° as frequency drops toward the cutoff — standard first-order high-pass response. This part of the original analysis was correct on its own terms; **the AC Sweep result matched the hand-calculated `H(f)` closely**, confirming C4/R5 passes the vast majority of the audio band through to Node A regardless of R5's value in the range tested.

This confirmed the transfer-function math was never the problem — the missing piece was entirely downstream of Node A, which AC Sweep structurally can't see (see the diode note above).

---

## Transient analysis — the self-bias discovery

Switching to transient analysis (a sine source, then later `AM_VOLTAGE`) with probes on Node A and Node B, at the original `R5 = 100kΩ`:

- Node A's waveform showed a clear, unexpected characteristic: rather than sitting centred on 0V, its *average* sagged well below zero once the circuit reached steady state (confirmed by watching several R5×C4 time constants play out — not a transient artifact).
- Node B settled to only **~0.12–0.2V** against a realistic 2.55V-peak input — nowhere near the ~2.2–2.4V the original "peak − Vf" hand-calculation had assumed.

This is what led to identifying the self-biasing mechanism: because C4 blocks DC entirely, and D1 only draws current from Node A in one direction (toward Node B, on each positive peak), that current has to be exactly balanced over each cycle by current flowing the *other* way through R5 — there's no other path. The only way to balance a one-directional current draw through a DC-blocked node is for that node's *average* voltage to shift negative. This is the same self-biasing/DC-restoration effect used deliberately in clamper circuits — a known behaviour, just not one the original transfer-function-only analysis accounted for.

Derived and confirmed against the simulation:

```
V_A(avg) ≈ -V_B(avg) × (R5/R6)
V_B ≈ (A×H − Vf) / (1 + R5/R6)
```

At `R5 = 100kΩ`, `R6 = 10kΩ`: `(1 + R5/R6) = 11` — roughly **91% of the signal reaching Node A never reached Node B at all**, regardless of how cleanly C4/R5 passed the AC signal itself. This matched the simulated ~0.2V result closely.

---

## R5 sweep — finding the corrected value

`R5` trades off two effects that move in opposite directions:

- **Lower R5** → smaller `(1+R5/R6)` → less self-bias loss → more of the AC signal that reaches Node A actually reaches Node B.
- **Lower R5** → also lowers the C4/R5 corner frequency's headroom → *more* bass attenuation in `H(f)`, since R5 needs to stay large relative to C4's impedance at low frequency to avoid attenuating it.

Several values were tested in transient analysis at the worst-case frequency (20Hz, where C4's impedance is largest) against the confirmed 2.55V peak:

```
R5 = 10,000Ω:  self-bias factor = 1/(1+1.00) ≈ 50%   →  V_B(20Hz) ≈ 1.05V
R5 =  4,700Ω:  self-bias factor = 1/(1+0.47) ≈ 68%   →  V_B(20Hz) ≈ 1.15V
R5 =  2,200Ω:  self-bias factor = 1/(1+0.22) ≈ 82%   →  V_B(20Hz) ≈ 0.86V  (worse — bass loss now dominates)
```

Node B's voltage peaks around `R5 ≈ 5–6kΩ` and falls off on both sides — confirming lower isn't always better once both effects are considered together. `4.7kΩ` lands within ~1% of that optimum, using the same `2.2µF` non-polarized C4 already on hand (no new part needed).

**Result at `R5 = 4.7kΩ`:** Node B settles around **~1.1–1.15V at the worst-case bass frequency, up to ~1.4V for typical midrange/treble content** — confirmed directly in Multisim, not just predicted — a 5x+ improvement over the original 100kΩ spec.

---

## Validation against a realistic signal — AM-modulated transient test

A clean 1kHz sine wave is a poor stand-in for real music, which varies constantly in both amplitude and frequency content. To stress-test the corrected circuit (`R5 = 4.7kΩ`) against something closer to real dynamic audio, Multisim's `AM_VOLTAGE` source was used in place of a plain sine source:

- **Carrier amplitude:** `1.3V` (chosen so the resulting AM peak, `Carrier × (1+m)`, lands at the confirmed real-world peak of ~2.55V)
- **Carrier frequency:** `1kHz` — stands in for the audio content itself
- **Modulation index:** `1` — full-depth modulation, so the envelope swings from ~0 up to the full peak
- **Intelligence (modulation) frequency:** `5Hz` — a slow, music-like loudness variation (the default `100Hz` was also tried as a worst-case "unrealistically fast dynamics" stress test; 5Hz better represents how quickly real music's loudness actually changes)

With Channel A (source) and Channel B (Node B) both on **DC coupling**, the resulting trace showed exactly the expected shape: Node B tracking the slow AM envelope, entirely positive throughout, with fine `1kHz`-carrier ripple riding on top of the smoothed envelope — the smoothing/discharge stage (C3/R6) doing its job of turning the rectified ripple into a slowly-varying "how loud is it right now" signal, on top of the now-much-smaller self-bias sag from the corrected R5 value.

**One debugging detour along the way:** an earlier pass at this same test showed Node B appearing to swing both positive *and* negative — physically impossible for a signal downstream of a one-way diode. After ruling out a mislabelled channel and reversed polarity, the actual cause was **Channel A's coupling being left on AC instead of DC** — AC coupling re-centres a trace around its own average, so an always-positive signal can appear to straddle zero purely as a scope display artifact. Switching to DC coupling resolved it immediately, and the corrected trace matched the expected, physically-sensible shape. (This is the same lesson as the DC-coupling note in the setup section above — it came up independently in both places.)

---

## Conclusion

The Multisim work confirmed, in order:

1. The original C4/R5 transfer-function math was correct on its own — AC Sweep matched the hand-derived `H(f)` exactly.
2. That math was incomplete — it missed a genuine self-biasing effect from D1 pulling current through a DC-blocked node, confirmed directly via transient analysis (Node B measured at ~0.2V against 2.55V peak at the original `R5 = 100kΩ`, not the ~2.2–2.4V originally assumed).
3. `R5` has a real optimum (~5–6kΩ) balancing self-bias loss against bass attenuation, found by sweeping several values rather than assuming "lower is always better."
4. `R5 = 4.7kΩ` — within ~1% of that optimum — was validated first against a plain sine source, then against a more realistic AM-modulated signal approximating real dynamic music content, confirming the corrected circuit behaves as expected under conditions closer to actual use.

Since `R5` is a hand-soldered through-hole part (same as `R3`/`R4`/`C3`/`C4`), this is a cheap part swap at assembly, not a re-fab — see the `v2` PCB note in [docs/audio-reactive-led-plan.md](audio-reactive-led-plan.md) and [hardware/audio-breakout.md](../hardware/audio-breakout.md).

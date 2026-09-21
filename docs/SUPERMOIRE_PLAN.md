# New variation: `supermoire`

## Context

The user shared a Nature Materials News & Views article ("Visualizing the moiré of moiré", Ning & Gedik, 2024) about *supermoiré* patterns: second-order interference lattices that emerge when two moiré superlattices — each itself formed by twisting a 2D lattice by a small angle — overlap because their periodicities are close but not identical. The paper's key findings, translated into flame-variation terms:

- Each moiré is well modeled as a triangular-symmetry potential: a sum of plane waves at 120° apart, `V(r) = Σ cos(G_i · r)` for three reciprocal-lattice directions.
- A "supermoiré" arises from a **nonlinear** combination of two such lattice potentials (the paper states explicitly that a linear sum of the two moiré potentials is not enough to reproduce observed data — it needs a nonlinear addition).
- A small **uniaxial heterostrain** applied to one lattice drives a geometric transition between three regimes: triangular lattice → quasi-square lattice → one-dimensional fringe lattice (Fig. 1b–d).
- The two lattices are also parameterized by independent **twist angles** and near-equal **periodicities/frequencies**, whose mismatch sets the supermoiré's beat period.

This maps naturally onto a new fractal-flame **variation**: a periodic/lattice-interference coordinate warp with parameters for two lattice frequencies, two twist angles, a strain, and a nonlinear mixing coefficient — genuinely novel among existing variations (confirmed no `moire`/`lattice`/`supershape`/`modulus`-style variation exists in this codebase today) and visually in the same family as existing periodic variations (`waves`, `waves2`, `rings2`).

## Design

New variation `supermoire` (`Var*` pattern, registered/non-local — NOT added to `XForm.cpp`'s local-variation list).

**Math** — private helper, a 3-fold ("triangular lattice") plane-wave sum:

```
triLattice(x, y, angle, freq) = Σ_{k=0..2} cos( freq * (x*cos(angle + k*2π/3) + y*sin(angle + k*2π/3)) )
```

`calc()` builds two lattices — lattice 2 sampled through a uniaxial-strain-warped coordinate (`x*(1+strain), y*(1-strain)`) — and combines them nonlinearly as the paper describes, then uses that scalar field to displace `tx`/`ty` (same additive-Cartesian-warp convention as `VarWaves2`). To get a genuinely 2D (non-separable) displacement from one scalar-field family, the y-channel resamples the same lattices with `x`/`y` swapped (same trick spirit as `waves2` using `sin(ty*freqx)` for the x term and `sin(tx*freqy)` for the y term):

```cpp
double S(double x, double y) const {
    double L1 = triLattice(x, y, angle1_, freq1_);
    double L2 = triLattice(x * (1 + strain_), y * (1 - strain_), angle2_, freq2_);
    return L1 + L2 + mix_ * L1 * L2;
}

void calc() {
    *px += vvar * (*tx + scale_ * S(*tx, *ty));
    *py += vvar * (*ty + scale_ * S(*ty, *tx));
    *pz += vvar * *tz;   // pass-through, matches VarRings2's 2D-only convention
}
```

**Parameters** (plain `double` members, trailing underscore, `supermoire_`-prefixed names, no min/max metadata — matches codebase convention):

| name | member | default | meaning (paper analogue) |
|---|---|---|---|
| `supermoire_freq1` | `freq1_` | 8.0 | lattice 1 spatial frequency (∝ 1/period, θ_TBG lattice) |
| `supermoire_freq2` | `freq2_` | 9.0 | lattice 2 spatial frequency (θ_hBN lattice; mismatch vs freq1 sets beat period) |
| `supermoire_angle1` | `angle1_` | 0.0 | lattice 1 twist angle (radians) |
| `supermoire_angle2` | `angle2_` | 0.12 | lattice 2 twist angle (radians) |
| `supermoire_strain` | `strain_` | 0.0 | uniaxial heterostrain ε (drives triangular→square→1D transition) |
| `supermoire_mix` | `mix_` | 0.5 | nonlinear mixing coefficient λ for the `L1*L2` interference term |
| `supermoire_scale` | `scale_` | 0.2 | overall displacement amplitude |

`kName = "supermoire"`, `kSupports3D = false`, `kSupportsDC = false` (2D lattice concept; z passes straight through as shown above, matching `VarRings2`'s handling of `pz`).

`getVariable`/`setVariable`/`resetVariable`/`variableNameAt`: plain if-chains, same shape as `VarWaves2.cpp:15-55`. No special value normalization needed (no wraparound/clamping required — matches `VarWaves2`, unlike `VarLazysusan`'s angle-wrap case).

## Files to add/touch

- `src/core/variations/VarSupermoire.h` — class declaration, modeled on `src/core/variations/VarWaves2.h`.
- `src/core/variations/VarSupermoire.cpp` — implementation as above, modeled on `src/core/variations/VarWaves2.cpp:1-61`; ends with `namespace { const bool kRegistered = registerVariation<VarSupermoire>(); }`. No special registration flags needed (no RNG side effect in `prepare()`, no non-deterministic construction default — all params are fixed constants).
- No edits needed to `CMakeLists.txt` (glob picks up the new files automatically — `src/core/CMakeLists.txt:10`), `VariationRegistry`, `XForm.cpp`, or `FlameIO.cpp` (XML read/write is fully generic over `variableNameAt()`).
- New test file `tests/variations_supermoire_test.cpp` (glob-picked via `tests/CMakeLists.txt:5`'s `*_test.cpp` suffix), modeled on `tests/variations_batch9_test.cpp`'s pattern (`registeredIndex()` helper via `VariationRegistry::instance().variationIndex(...)`, an `XForm` + `MtRng` + `prepare()`/`nextPoint()` harness):
  - `supermoire` is registered and has 7 variables with the expected names.
  - A hand-computed value check: set `tx=0.3, ty=0.4` (or similar), default params, and assert `px`/`py` match an independently re-derived expected value (compute `S(x,y)` by hand from the formula above, not by copy-pasting `calc()`'s own code — matching the "independent re-derivation" spirit of `variations_batch9_test.cpp`).
  - `resetVariable` restores each parameter's documented default.
  - Sanity: `strain_ = 0` reduces cleanly to the unstrained two-lattice sum (no special-casing needed to verify — just confirms the formula, since strain only rescales lattice-2's sampled coordinates).

## GPU/CUDA support (in scope)

`waves2`/`rings2`/`sinhpowia` etc. show the exact 4-file pattern for porting a registered variation to the optional CUDA backend (only compiled/relevant when `APO_ENABLE_CUDA` is on; guarded by `#ifdef`/build option elsewhere, not something this change needs to touch). `supermoire`'s math (trig + one multiply, no branches, no RNG, no `prepare()`-cached state) is a straightforward "Batch 4/5-style" port — same shape as `kSinhPow`/`kSinhPowIA` (`src/core/render/gpu/VariationKinds.h:125-128`): device `calc()` recomputes everything inline from raw params each call, no host-side precompute step needed.

- `src/core/render/gpu/VariationKinds.h`: add `constexpr int kSupermoire = kFirstRegisteredKind + 67;` after `kSinhPowIA` (line 128), inside `namespace kind`.
- `src/core/render/gpu/VariationKinds.cpp`: add a `kRegisteredVarTable` entry, param order matching `VarSupermoire::variableNameAt()` exactly (same convention as `VarWaves2`'s entry, `VariationKinds.cpp:40-41`):
  ```cpp
  {"supermoire", {kind::kSupermoire, {"supermoire_freq1", "supermoire_freq2", "supermoire_angle1",
                                       "supermoire_angle2", "supermoire_strain", "supermoire_mix",
                                       "supermoire_scale"}}},
  ```
- `src/core/render/gpu/DeviceVariations.cuh`: add a `devSupermoire(DeviceVarContext& c)` device function mirroring `VarSupermoire::calc()`'s math via `c.params[0..6]`/`c.tx`/`c.ty`/`c.tz`/`c.vvar`/`c.px`/`c.py`/`c.pz` (same shape as `devWaves2`, `DeviceVariations.cuh:400-406`), plus one `case kind::kSupermoire: devSupermoire(c); return;` line in the dispatch switch (alongside `case kind::kWaves2:` etc., `DeviceVariations.cuh:1259`). A shared `__device__` helper (or inlined 3x per call) for the `triLattice()` plane-wave sum keeps this from duplicating the CPU-side formula by hand twice within the same function.
- `tests/gpu_variation_parity_test.cpp`: append `"supermoire"` to the manual list of GPU-ported variation names (`tests/gpu_variation_parity_test.cpp:181-201`, e.g. as a trailing "Batch 5" entry after `"sinhpow", "sinhpowia",`) — this is what drives that test's existing generic CPU-vs-GPU randomized-parameter parity check for the new variation, with no other special-casing required.

This only builds/runs when CUDA is enabled locally; if this machine's build doesn't have `APO_ENABLE_CUDA` on, these 4 edits still compile-check as ordinary C++/host code changes but the parity test itself won't execute here — call that out rather than silently skipping verification.

## Verification

1. Build: configure/build the project (CMake) so the new glob-discovered files compile into `apo_core` and the test target.
2. Run the new `variations_supermoire_test` binary (and the full test suite) to confirm registration, parameter round-trip, and the hand-computed `calc()` value all pass.
3. If this environment has `APO_ENABLE_CUDA` on: rebuild with CUDA enabled and run `gpu_variation_parity_test` (which now includes `"supermoire"`) to confirm the device `devSupermoire()` matches the CPU `VarSupermoire::calc()` across randomized parameters. If CUDA isn't available here, say so explicitly rather than claiming this was verified.
4. Manual visual check: open the app (or use whatever existing flame-render CLI/test harness renders a single xform to PNG), create an xform using only `supermoire` at weight 1, and render at a few parameter presets (default; `strain_` swept from 0 → ~0.3 to confirm the triangular→square→fringe-like visual transition described in the paper; `mix_ = 0` vs `mix_ = 0.5` to see the effect of the nonlinear term) to confirm the pattern looks like the intended lattice-interference structure and contains no NaN/degenerate output.

## How it works in math

Step 1: One "lattice" from three ripples

Imagine dropping three straight-line ripples across a pond, each going in a different direction, spaced 120° apart. Where the ripples overlap constructively (all three peaks align), you get bright spots; where they cancel, you get dark gaps. That produces a honeycomb/triangular grid of bright dots — this is literally how physicists model a single moiré lattice's potential:

triLattice(x, y) = cos(wave 1) + cos(wave 2) + cos(wave 3)

Each "wave" is just cos(frequency × distance along that direction). The angle parameter rotates all three ripple directions together (this is the "twist angle" from the paper), and freq controls how tightly packed the grid is (the lattice period).

Step 2: Two lattices, slightly mismatched

Now do that twice — lattice 1 and lattice 2 — with almost the same frequency but not quite, and a slightly different twist angle. Two nearly-identical grids overlaid on top of each other produce a much larger, slower-moving pattern where their misalignment drifts in and out of sync — that's the classic "moiré" effect you see when two window screens overlap. That's L1 and L2 in the code.

The strain parameter squishes lattice 2 slightly along one axis before computing it (stretch x, compress y) — this is the "heterostrain" from the paper, and it's what warps the grid from triangular toward a stretched/square/stripy shape.

Step 3: Combine them the "wrong" (nonlinear) way

The paper's key finding was that just adding the two lattice patterns (L1 + L2) isn't enough to match what they saw experimentally — you need a nonlinear term too, i.e. also multiply them together:

field = L1 + L2 + mix × (L1 × L2)

The multiplication term is what actually creates the richer "moiré of moiré" structure — it's largest exactly where both lattices are simultaneously bright, carving out a whole new, larger-scale interference pattern from the interaction between the two.

Turning that into a fractal transform

Flame variations just need a rule "given a point (x, y), where does it move to?" So I use that field value as a displacement:

new_x = x + scale × field(x, y)
new_y = y + scale × field(y, x)   ← same formula, x/y swapped

Swapping x and y for the second line is a cheap trick (borrowed from how the existing waves2 variation works) to get a genuinely two-dimensional warp out of one scalar pattern, rather than just stretching uniformly.

When this gets applied repeatedly inside the fractal's iteration (mixed with two other simple contracting transforms to keep everything bounded), the lattice-interference ripples get nested inside themselves over and over — which is why the strain sweep you saw earlier produced such different-looking, self-similar fringe structures at each setting.

## In the UI:

Here are concrete presets to plug into the UI's Variables tab for supermoire, plus a quick scaffold so the flame stays well-behaved.

Setup scaffold (do this once):
Create 3 transforms arranged like a triangle (Adjust panel → set each xform's position to one corner, Scale ≈ 0.5):
- Xform 1: position ≈ (-0.5, -0.5), scale 0.5 — this is the one that gets supermoire
- Xform 2: position ≈ (0.5, -0.5), scale 0.5 — leave as plain linear
- Xform 3: position ≈ (0, 0.5), scale 0.5 — leave as plain linear

On Xform 1, turn off linear (weight 0) and turn on supermoire at weight 1. The other two xforms act as "anchors" that keep the chaos game contracting/bounded even though supermoire itself doesn't shrink points — I verified this combo renders cleanly.

Presets to try in the Variables tab (confirmed by render):

┌──────────────────┬───────┬───────┬────────┬────────┬────────┬─────┬───────┬─────────────────────────────────────┐
│      Preset      │ freq1 │ freq2 │ angle1 │ angle2 │ strain │ mix │ scale │                Look                 │
├──────────────────┼───────┼───────┼────────┼────────┼────────┼─────┼───────┼─────────────────────────────────────┤
│ Triangular       │ 8     │ 9     │ 0      │ 0.12   │ 0      │ 0.5 │ 0.2   │ Nested triangular ripples,          │
│ (default)        │       │       │        │        │        │     │       │ honeycomb-ish                       │
├──────────────────┼───────┼───────┼────────┼────────┼────────┼─────┼───────┼─────────────────────────────────────┤
│ Transition       │ 8     │ 9     │ 0      │ 0.12   │ 0.15   │ 0.5 │ 0.2   │ Skewed, more clawed/angular fringes │
├──────────────────┼───────┼───────┼────────┼────────┼────────┼─────┼───────┼─────────────────────────────────────┤
│ Fringe stripes   │ 8     │ 9     │ 0      │ 0.12   │ 0.35   │ 0.5 │ 0.2   │ Strong directional banding, very    │
│                  │       │       │        │        │        │     │       │ "woven" look                        │
└──────────────────┴───────┴───────┴────────┴────────┴────────┴─────┴───────┴─────────────────────────────────────┘

Worth experimenting with (same math, untested combos — reasoned from the formula, so treat as starting points rather than guarantees):

- Bigger beat pattern: bring freq1/freq2 closer together, e.g. freq1=8, freq2=8.3 — a smaller frequency mismatch means the two lattices drift in/out of alignment more slowly, so the "supermoiré" beat structure shows up at a much larger scale (fewer, bigger interference cells instead of many small ones).
- Busier/finer pattern: push them further apart, e.g. freq1=6, freq2=11.
- Stronger nonlinear character: crank mix up to 1.5 or 2 (the paper's whole point was that the multiplicative term matters — pushing it harder should make that interaction term dominate over the plain sum).
- More dramatic bidirectional fringes: widen the twist mismatch, e.g. angle2 = 0.5 (~28°) instead of 0.12.
- Bolder/softer overall: scale directly controls displacement strength — try 0.35 for a bolder, more chaotic weave, or 0.1 for something subtler. If it blows out to mostly black/empty, that's scale pushing points out of the other xforms' contracting basin — pull it back down or lower the anchor xforms' Scale slider a bit (e.g. 0.45 instead of 0.5) to compensate.

Good order to explore: start from the triangular preset, sweep strain up first (biggest visual swing), then try a mix bump, then play with the frequency mismatch last.
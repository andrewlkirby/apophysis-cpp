# sinhpow / sinhpowia - two new variations from a 2026 Mandelbrot-set paper

## Where this came from

Tassaddiq, Tanveer, Alghamdi, Alharbi, Albarakati, Md Kasmani & Almutairi,
["A Visual and Quantitative Study of Fractal Mandelbrot Sets Using the
IA-Iterative Algorithm for Complex
Functions"](https://www.mdpi.com/2504-3110/10/6/365), *Fractal Fract.* 2026,
10, 365.

That paper is **not** about fractal flames. It studies escape-time
Mandelbrot sets: for the transcendental map

```
T_c(z) = z^p + sinh(c^q),    p >= 2, q >= 1, c in C \ {0}
```

it derives a rigorous escape-radius criterion under a new fixed-point
scheme the authors call the "IA iteration" (four relaxation parameters
alpha, beta, gamma, lambda), then colors a fixed pixel grid by how many
iterations each parameter value `c` takes to escape - the classical
Mandelbrot-set generation paradigm (bounded orbit of a critical point vs.
divergent one), nothing like a chaos-game IFS.

That paradigm doesn't port to this codebase directly: fractal flames have
no escape/bounded criterion at all, and each xform's variation is evaluated
**exactly once** per chaos-game point per iteration, not iterated to
convergence. What *does* transfer is the map's own nonlinearity,
`z^p + sinh(c^q)` - the same shape of building block existing variations
like `julian`/`julia3Dz`/`mobius` are already made from. Two variations
came out of it, corresponding to the two ways of using that building block
in a single per-point `calc()` call:

- **`sinhpow`** - evaluate `T_c(z)` once per point (cheap).
- **`sinhpowia`** - run one full step of the paper's own IA iteration per
  point, i.e. four evaluations of `T_c` (expensive, richer texture).

Both are fully native (CPU + CUDA), tested, and registered like any other
variation - see `src/core/variations/VarSinhPow.{h,cpp}` and
`VarSinhPowIA.{h,cpp}`.

## sinhpow

`T_c(z) = z^p + sinh(c^q)`, evaluated once per point. `z = (tx, ty)` is the
pre-affine-transformed input point; `p`, `q`, `c` are per-xform parameters,
not iterated.

Key implementation trick: `c` and `q` are fixed parameters, not the point,
so `sinh(c^q)` is a **constant complex number** for the whole xform - it
doesn't depend on `z` at all. `p` and `q` are treated as continuous real
exponents via the principal branch of complex exponentiation
(`z^w = exp(w * log(z))`), the same generalization `VarPower.cpp` already
makes for its own single real exponent, extended here to both.

Parameters (all continuous real, no RNG involved anywhere in this
variation):

| name             | meaning  | default |
|------------------|----------|---------|
| `sinhpow_power`  | p        | 2.0     |
| `sinhpow_q`      | q        | 1.0     |
| `sinhpow_cx`     | Re(c)    | 1.0     |
| `sinhpow_cy`     | Im(c)    | 0.0     |

## sinhpowia

One full step of the paper's own IA iteration (Definition 3 / Theorem 1),
per point:

```
x  = (alpha+beta)*z + gamma*T_c(z)
w  = (1-lambda)*T_c(x) + lambda*T_c(z)
y  = T_c(w)
z' = T_c(y)
```

Four evaluations of `T_c` per point instead of `sinhpow`'s one - roughly
4x the per-point cost, paid on every chaos-game iteration.
`alpha`/`beta`/`gamma`/`lambda` are exposed as four **independent** free
parameters, not constrained to `alpha+beta+gamma=1` at runtime the way the
paper's own escape-time use requires: flame parameters are continuously
user-dragged in the editor, the map is well-defined algebraically for any
real values, and enforcing the constraint server-side would just make the
parameters fight the UI.

Parameters:

| name                 | meaning | default |
|----------------------|---------|---------|
| `sinhpowia_power`    | p       | 2.0     |
| `sinhpowia_q`        | q       | 1.0     |
| `sinhpowia_cx`       | Re(c)   | 0.3     |
| `sinhpowia_cy`       | Im(c)   | 0.2     |
| `sinhpowia_alpha`    | alpha   | 0.3     |
| `sinhpowia_beta`     | beta    | 0.3     |
| `sinhpowia_gamma`    | gamma   | 0.4     |
| `sinhpowia_lambda`   | lambda  | 0.25    |

### Why `c` defaults to `0.3+0.2i`, not the paper's own `c=1`

`alpha`/`beta`/`gamma`/`lambda`'s defaults match one of the paper's own
example parameter sets (their Figure 2). `c`'s default deliberately does
**not** match the paper's `c=1` - that was verified empirically, not
assumed, by rendering an actual test flame. With `c=1`, four compounded
`T_c` evaluations diverge past the render canvas for essentially every
starting point: the chaos game's initial fuse/settle phase never finds a
point that stays bounded, and the render comes back with **0 points
generated**. A freshly-added `sinhpowia` xform at the paper's own default
would render as a blank image.

The reason this doesn't contradict the paper: their `c=1` only ever needs
to stay bounded under their own escape-time check - a fixed pixel grid
testing every candidate `c` independently, with a generous escape radius
and a bounded iteration cap. A flame variation instead needs one specific
`c` to sustain a **self-referential** chaos-game orbit indefinitely under
four compounded evaluations per step - a materially different (and much
stricter) boundedness requirement. `c = 0.3+0.2i` was verified to render a
real, non-degenerate attractor (tens of millions of points landing inside
the frame). Larger `power` values need proportionally smaller `c` still -
`power=3` with otherwise-small parameters was observed to blow up
spatially (numerically finite, no NaNs, but almost every point landing
outside the visible frame) even though `sinhpow`'s single-evaluation form
handles `power=3` fine at the same `c`.

## Performance / GPU

- `sinhpow`: same cost class as `julian`/`julia3Dz` - one complex power
  plus a handful of trig calls.
- `sinhpowia`: ~4x `sinhpow`'s per-point cost (four `T_c` evaluations
  instead of one).

Both are ported to CUDA (`devSinhPow`/`devSinhPowIA` in
`src/core/render/gpu/DeviceVariations.cuh`, kind ids and param tables in
`VariationKinds.h`/`.cpp`). Neither uses RNG or a `prepare()`-cached
constant - `sinh(c^q)` is recomputed inline from the raw parameters on
every call, the same convention the existing "prepare()-only" batch of
registered variations already uses (see `VariationKinds.h`'s own comment
on that batch) - so there's no host-side precompute/upload path to keep in
sync.

CPU-vs-GPU parity (`tests/gpu_variation_parity_test.cpp`, PSNR against the
CPU-rendered reference, independent RNG streams so this is a shape/tone
match, not bit-exactness):

| variation   | PSNR     |
|-------------|----------|
| `sinhpow`   | 53.3 dB  |
| `sinhpowia` | 59.3 dB  |

both comfortably above the 15 dB pass bar and in line with every other
ported variation's 36.9-63.4 dB range.

## Tests

- `tests/variations_batch8_test.cpp` - `sinhpow`: registration, a
  hand-computed `z^2 + sinh(1)` check at a known point, a `c=0`/origin
  finite-output (no NaN/inf) check, parameter get/set round-trip.
- `tests/variations_batch9_test.cpp` - `sinhpowia`: registration, an
  expected-value check computed independently via `std::complex`/`std::pow`
  (deliberately not transcribing the production code's own real/imaginary-
  pair arithmetic, so it's a genuinely different code path than what it's
  checking), the same `c=0`/origin finite-output check, parameter
  round-trip.
- `tests/gpu_variation_parity_test.cpp` - both added to the CPU-vs-GPU
  parity list.
- `tests/variation_roundtrip_test.cpp` - both covered automatically (it
  iterates every registered variation): 143/143 round-trip through flame
  XML I/O.

## Files touched

- `src/core/variations/VarSinhPow.{h,cpp}`, `VarSinhPowIA.{h,cpp}` (new)
- `src/core/render/gpu/DeviceVariations.cuh` (`devSinhPow`, `devSinhPowIA`,
  `devComplexPow` helper, dispatch cases)
- `src/core/render/gpu/VariationKinds.h`/`.cpp` (kind ids, param tables)
- `tests/variations_batch8_test.cpp`, `variations_batch9_test.cpp` (new)
- `tests/gpu_variation_parity_test.cpp` (both added to the parity list)

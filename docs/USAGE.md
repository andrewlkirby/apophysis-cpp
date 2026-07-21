# Using Apophysis 7X

A quick tour of the app itself — the library window, the flame editor, and
the render/gradient/randomization tools around them. This is a user guide,
not a build guide; see the [README](../README.md) for building from source.

## The Library window

This is the window you land in when you launch `apo_gui.exe`. It holds every
flame currently loaded — a scrollable list on the left, a live preview of
the selected flame on the right.

- **File > New Flame** (`Ctrl+N`) — adds a single blank flame to the
  library.
- **File > New Random Batch...** (`Ctrl+B`) — generates a batch of random
  flames in one go, using whatever ranges/weights are configured on the
  **Random** tab of Options (see below). Useful for exploring starting
  points instead of designing one from scratch.
- **File > Open...** (`Ctrl+O`) — loads flames from a `.flame` file
  (Apophysis' XML parameter format); a file can contain more than one flame,
  and all of them are added to the library.
- **File > Save Flame As...** (`Ctrl+S`) — saves the *selected* flame.
  **File > Save All Parameters...** saves every flame currently in the
  library to one `.flame` file.
- **File > Save Render As...** — renders the selected flame at its current
  settings and saves the result straight to a PNG, without opening the full
  Render dialog.
- **File > Render All Flames...** — opens a dedicated dialog to batch-render
  every flame in the library to PNGs in one pass.
- **File > Smooth Palette...** — generates a smooth color gradient from the
  selected flame's histogram (enabled once a flame has been rendered at
  least once).

Double-click (or select and press **Library > Edit Flame**) to open a flame
in the **Editor** window described below. Right-click a library entry for a
context menu with Edit/Duplicate/Rename/Delete and Reset Location.
**Library > Reset Location** (`F12`) resets just the camera (zoom/pan) back
to the flame's default framing.

The preview pane itself is interactive: scroll to zoom (anchored on the
cursor), click-drag to pan — both edit the flame's real camera directly, so
what you see is what a full render will produce.

Toggle **Thumbnails** vs **List** view from the toolbar — thumbnails render
a small preview per entry (nice for browsing, a bit slower for very large
batches); List mode skips that and is just names, useful once a batch gets
large.

**Edit > Copy Flame / Paste Flame** copies the selected flame's full
parameters to the clipboard and back, including between separate runs of
the app. **Edit > Undo/Redo** (`Ctrl+Z` / `Ctrl+Y`) cover library-level
operations (new/delete/duplicate/paste, etc.) — the Editor window has its
own, separate undo/redo stack for the flame currently being edited (see
below).

## The Editor window

Opened from the library by double-clicking a flame. Three panes:

- **Left** — the list of the flame's transforms ("Xforms"), plus an
  optional **Final** transform (applied once per point, after every
  iteration, never chosen probabilistically — toggle it with the
  **Final Transform** toolbar button).
- **Center** — the **triangle canvas**: each transform is shown as a
  draggable triangle. Dragging a vertex or edge edits that transform's
  affine coefficients live, with the preview re-rendering as you go.
- **Right** — the transform panel, with tabs for the selected transform's
  **Variations** (weights, and each variation's own parameters),
  **Colors** (color/color speed/opacity/direct color, plus a Solo toggle to
  preview just this transform's color), and **Transform** (exact numeric
  coefficients, rotate/scale-by-degrees/percent tools, flip, and an
  "Edit post transform" toggle that switches every canvas/panel edit onto
  the transform's post-transform instead).

Canvas toolbar modes: **Move**, **Rotate**, **Scale** (which gesture a drag
performs), **Post** (edit the post-transform instead of the main one — kept
in sync with the panel's own checkbox), **Set Pivot**/**Clear Pivot** (pick
a custom rotate/scale pivot point on the canvas instead of the triangle's
own center), and **Fit View**.

Transform management: **Add**, **Duplicate** (also `Insert`), **Delete**
(also `Del`), **Copy**/**Paste** (coefficients, variations, colors, and
name — paste onto whichever transform is currently selected).

Other toolbar tools:
- **Xaos...** — per-transform-pair transition probabilities (how likely one
  transform is to follow another, rather than every transform being equally
  likely every iteration).
- **Force Symmetry...** — adds transforms that impose rotational/reflective
  symmetry on the flame.
- **Adjust...** — camera (zoom/pan/angle/3D perspective), coloring
  (gamma/brightness/vibrancy/background), gradient editing, and canvas size,
  all in one dialog with its own live preview and its own undo/redo stack.
  Click the gradient strip itself (or the **Browse Gradients...** button) to
  open the gradient library and pick a different one; **Invert**,
  **Reverse**, **Randomize**, and **Reset** apply straight to the current
  gradient.
- **Render...** — full-quality PNG export; see "Rendering" below.
- **Mutate...** — generates a grid of nearby variants of the current flame
  to explore ("what if I nudged this in a random direction?"), pick one to
  adopt.
- **Curves...** — tone-curve color adjustment.
- **Fullscreen** — a distraction-free, large preview of the current flame.

The **Flame** menu has a few whole-flame utilities: **Random Weights** /
**Equalize Weights** (randomize or flatten every transform's probability
weight) and **Calculate Colors Values** / **Randomize Color Values**.

## Rendering

The **Render** dialog controls a final, full-quality export to PNG:
width/height (with size presets and a **Maintain Aspect Ratio** lock),
sample density, oversampling, filter radius, adaptive (density-estimation)
filtering, and transparent background. Click **Render** to start; a
**Pause** button lets you genuinely pause and resume an in-progress render
(the render thread blocks in place — nothing already computed is lost),
and **Cancel** stops it early (still saving whatever was rendered so far).
Check **Save parameters (.flame) alongside the image** to also write out a
`.flame` file with the exact settings used, next to the PNG. Once a render
finishes successfully, **Post Process...** becomes enabled — it opens a
dialog for tuning tone-mapping (gamma/brightness/contrast/vibrancy/filter
radius/background) on the image that was just rendered, without touching
this flame's own saved settings; use its own **Save** button to export the
adjusted PNG.

**Render All Flames...** runs the same render settings across every flame
in the library in one batch, useful for exporting a whole random-batch
session at once.

## Gradients

The **Gradient Browser** (opened from the Adjust dialog, or by clicking the
gradient strip there) lets you browse and load gradients from `.ugr`
gradient library files — the same format the original Apophysis and flam3
tools use. Once a gradient is applied, the Adjust dialog's Gradient tab
supports further tweaks (rotate, hue/saturation/brightness/contrast shift,
blur, frequency, invert, reverse) plus one-click Randomize.

## Randomization tools

**File > New Random Batch...** and the **Mutate** dialog both draw on the
same configurable randomization settings, found in **Tools > Options...**:

- **General** — miscellaneous app preferences.
- **Random** — xform count range, which variations are eligible to be
  picked, how many variations may be blended onto a single transform and
  their weight range, and whether each chosen variation's own parameters
  get randomized too (and by how much).
- **Variations** — enable/disable individual variations from being chosen
  by any random draw.
- **New Flame** — default settings applied to freshly created/opened
  flames (render quality, etc.) so the library stays fast to browse and
  preview.

## Command-line rendering

For scripted/headless rendering without the GUI, see `apo_render_cli` in
the [README](../README.md#headless-rendering).

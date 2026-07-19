# Building Apophysis 7X on Windows

This is a Delphi/VCL desktop application, so "running it" means compiling it
with a Delphi IDE first - there's no interpreter or portable runtime to just
double-click. This doc gets you from source to a working `.exe`.

## What's in this package

This is the full Apophysis 7X source tree with the optimization patches from
our work applied (see `OPTIMIZATION_STATUS.md` for the complete record of
what changed and why). Nothing else has been modified - this is otherwise the
unmodified upstream source.

## Prerequisites

1. **A Delphi IDE that can open a `.dproj` file (RAD Studio / Delphi 10.x
   or newer).** The project file targets `ProjectVersion 18.8`
   (Delphi 10.2 Tokyo), and the repo's own `build.cmd` references
   `Embarcadero\Studio\20.0` (RAD Studio 10.4 Sydney) - but you don't need
   that exact version. Any reasonably recent Delphi will offer to upgrade
   the project file automatically when you open it; accept that prompt.

   **Delphi Community Edition is free** for individuals and small
   companies/teams (check current eligibility terms on Embarcadero's site)
   and includes everything needed here - full IDE, VCL, Win32/Win64 compiler.

2. **VCL support** - included by default in any Delphi edition, including
   Community Edition.

3. **The TMS Scripter Studio dependency - read this before building.**
   See the dedicated section below. **This is the one real blocker** - the
   project will not compile without addressing it in some way.

## The TMS Scripter Studio dependency - good news, it's already disabled

**Correction from an earlier version of this document:** we initially said
TMS Scripter Studio (a commercial component behind the app's optional
scripting/automation feature) was a hard, non-optional build requirement.
That was wrong - we hadn't yet checked `Apophysis7X.dproj`'s conditional
compilation settings. It turns out **this exact project file already
defines `DisableScripting` in its base configuration, which applies to every
platform/configuration combination (Win32, Win64, Debug, Release)** - and
both `Apophysis7X.dpr` and `Forms\Main.pas` already have complete
`{$ifdef DisableScripting}` guards around every reference to the scripting
feature (the script editor forms, the "Preview" and "Favorites" windows that
depend on it, etc.).

**Net effect: building this project exactly as it stands, with zero code
changes, already skips TMS Scripter Studio entirely.** You don't need to
install anything, buy anything, or strip anything out - it's already
configured this way. If you ever want the scripting feature *back*, that
would mean removing `DisableScripting` from the `.dproj`'s base
`PropertyGroup` and installing TMS Scripter Studio (a free trial is
available via tmssoftware.com or Embarcadero's GetIt package manager) - but
that's opt-in, not something you need to do to get a working build.

Two other dependencies the README mentions also turned out to already be
vendored in the source tree, needing **no action**: the XML parser
(`System/NativeXml*.pas`) and the PNG library (`System/pngimage.pas`,
`pngextra.pas`) are both fully included. Indy isn't referenced anywhere in
the code despite being listed in the README, and ships bundled with Delphi
regardless, so it's a non-issue either way.

## Can I build this with VSCode instead of the Delphi IDE?

Partially, with a caveat worth knowing upfront: **VSCode has no Delphi/Object
Pascal compiler built in.** You'd still need Delphi (or just Delphi
Community Edition, free) installed on the machine for its command-line
compiler (`dcc32.exe`/`dcc64.exe`) and the RTL/VCL libraries - VSCode itself
can't produce a `.exe` from this source on its own, no matter what
extensions you add.

What VSCode *can* do:
- Edit the `.pas` files with reasonable syntax highlighting via a community
  Pascal/Delphi extension (search the VSCode marketplace for "Object
  Pascal" or "Delphi").
- Run the build by shelling out to the real Delphi compiler through a
  VSCode **task** (`.vscode/tasks.json`), using the same `msbuild`/`dcc32`
  command from the "Command line" build option below.
- Show compiler errors/warnings in VSCode's Problems panel, if the task's
  output is parsed with an appropriate `problemMatcher`.

What you'd be giving up compared to the full Delphi IDE:
- **The visual form designer** - this is a VCL app with many `.dfm` forms;
  if you ever need to visually add/move/resize a control, you'd be hand-editing
  the `.dfm` text format instead (they're plain text in modern Delphi, so
  it's *possible*, just more tedious and error-prone than dragging components
  in a designer).
- Integrated debugging is far more primitive - breakpoints/stepping through
  the Delphi debugger isn't something VSCode replicates without the IDE.
- Delphi-aware code completion/navigation (jump to declaration, etc.) is
  much weaker than what the real IDE or a paid extension provides.

Given none of the work we've done together involved visual form changes
(everything was direct `.pas` edits), VSCode-as-editor-plus-compiler-runner
would work fine for compiling and reviewing this specific set of changes.
If you end up needing to touch any `.dfm` file interactively, that's when
the full Delphi IDE becomes meaningfully more convenient.

## Build steps (once the TMS dependency is resolved)

### Option 1: Delphi IDE (recommended for first build)

1. Extract this package anywhere on disk.
2. Open `src\Apophysis7X.dproj` in the Delphi IDE. Accept any project
   upgrade prompt.
3. In the toolbar, set the **Platform** dropdown to **Win32** (see the
   note below on why Win32, not Win64) and **Configuration** to **Release**.
4. Build (Ctrl+F9) or Run (F9).
5. The resulting executable lands at `target\x86\Apophysis7X.exe`
   (relative to the repo root, per the project's configured output path).

### Option 2: Command line

The repo includes `build.cmd` at its root, but **it has a pre-existing typo**
on the Win64 line (`AApophysis7X.dproj` - doubled "A" - so it references a
file that doesn't exist and will fail if you reach that step). A corrected,
Win32-only version:

```bat
call "%PROGRAMFILES(X86)%\Embarcadero\Studio\<version>\bin\rsvars.bat"
msbuild /nologo /t:rebuild /verbosity:quiet /p:Platform=Win32 /p:Configuration=Release src\Apophysis7X.dproj
```

Replace `<version>` with whatever RAD Studio version number is actually
installed (check `%PROGRAMFILES(X86)%\Embarcadero\Studio\` to see what's
there).

### Why Win32, not Win64

The project's `.dproj` defaults to Win64 and is nominally configured to
support both platforms, but the rendering engine
(`Rendering\RenderingImplementation.pas`, `System\AsmRandom.pas`) contains
32-bit-specific inline x86 assembly (raw `EAX`/`EDX` register-based calls),
gated behind `{$define _ASM_}` with no `{$ifdef WIN64}` guard around it. The
repo's own `build.cmd` attempts a Win64 build, but given the typo bug found
in that exact line, it's unclear whether a Win64 build was ever successfully
verified. **Win32 is the safer, better-tested target** and is what the
assembly code was written against - start there. If you want to
investigate Win64 later, that inline assembly is the first place to look if
the build fails.

## Logging - what's captured, and where to find it

Before this, the app had **no active logging at all**. There's a
`TimeTrace` method on the renderer that looks like it logs render lifecycle
events (start/pause/progress messages), but the object it writes to
(`strOutput`) was never assigned anywhere in the entire codebase - every
call was a silent no-op. There was also no handler for unhandled
exceptions, so crashes only ever produced Delphi's default (unlogged) error
dialog.

We added a small logging unit (`src\System\AppLog.pas`) that fixes both:

- **Render trace messages** (`TimeTrace` calls throughout the renderer -
  "Rendering...", "Allocating buffers...", progress/pause events, etc.) are
  now also written to a persistent log file, in addition to the pre-existing
  (still otherwise-inert) `strOutput` mechanism.
- **Unhandled exceptions** are now caught via `Application.OnException`,
  logged with the exception class and message, and then still shown in
  Delphi's normal error dialog exactly as before - this only adds logging,
  it doesn't change what you see when something goes wrong.

**Log file location:** `%APPDATA%\Apophysis7X\apophysis7x.log` (falls back
to the folder containing the `.exe` if `%APPDATA%` isn't writable for some
reason). Each line is timestamped. The file is opened, appended to, and
closed on every single log write (rather than kept open for the app's whole
lifetime), specifically so that if the app crashes or is killed abruptly,
whatever was logged right before that is still flushed to disk - not lost
in a buffer.

**If something goes wrong and you want help debugging it:** send me the
contents of that log file (or just the tail end, around when the problem
happened) along with what you were doing at the time.

One limitation worth knowing: this logs the exception's class and message,
not a full stack trace - Delphi doesn't provide stack traces out of the box
without a separate library (e.g. madExcept, JCL Debug, or EurekaLog). If
crashes turn out to need deeper diagnosis than the message alone gives us,
adding one of those is a reasonable next step, just not something we've done
here.

## After building

The `.exe` is a self-contained rendering/editing app - no separate install
step needed beyond having built it. The `Plugins\` folder (present in the
originally-uploaded compiled distribution, not this source tree) is for
optional third-party variation DLLs and isn't required for a working build;
all the built-in variations are compiled directly into the executable from
`src\Variations\*.pas`.

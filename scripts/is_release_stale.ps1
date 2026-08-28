# Fast "does build-release/deploy/apo_gui.exe need rebuilding" check for
# launch_apo_gui_release.bat - deliberately NOT a `nmake`/`cmake --build`
# call, since even a true no-op nmake check costs ~2.5s on this project
# (Qt AUTOMOC's per-file dependency scanning over ~400 source files), and
# vcvars64.bat adds another ~1.5s on top - together enough to make every
# single app launch feel sluggish compared to double-clicking the exe
# directly. This script instead does its own newest-file-vs-exe comparison
# using raw .NET directory enumeration (Get-ChildItem's pipeline/object
# overhead is itself surprisingly costly at this file count - measured
# ~1.5s for the same scan), which finishes in well under 300ms.
#
# Exit code 0 = fresh (safe to launch the existing deploy\apo_gui.exe as-is).
# Exit code 1 = stale (something changed - go through the real build).
#
# Correctness note: do NOT swap the file-enumeration part of this script
# for `dir /s /o-d /b` as a "simpler" alternative - verified directly that
# `dir`'s /o-d sort is applied PER DIRECTORY, not globally across a /s
# recursive listing, so the first line of a multi-folder `dir /s /o-d /b`
# is only the newest file within whichever folder happened to be listed
# first, not the tree's actual newest file. That bug lets a real source
# change go undetected. Also do NOT pass a bare filename like
# "CMakeLists.txt" to `dir /s` expecting just the one top-level file - /s
# makes it search the entire subtree by name, which would incorrectly pull
# in every CMakeLists.txt under vcpkg_installed's build trees too.

param(
    [Parameter(Mandatory = $true)][string]$RepoDir,
    [Parameter(Mandatory = $true)][string]$DeployExe
)

if (-not (Test-Path $DeployExe)) {
    exit 1
}

$exeTimeUtc = [System.IO.File]::GetLastWriteTimeUtc($DeployExe)
$newestUtc = [DateTime]::MinValue

# src/ and cmake/ (recursive - anything here can affect the built binary)
# plus the top-level CMakeLists.txt (a single, exact file - not a
# recursive name search, see this script's own header comment on why).
$watchDirs = @((Join-Path $RepoDir 'src'), (Join-Path $RepoDir 'cmake'))
foreach ($dir in $watchDirs) {
    if (-not (Test-Path $dir)) { continue }
    foreach ($f in [System.IO.Directory]::EnumerateFiles($dir, '*', [System.IO.SearchOption]::AllDirectories)) {
        $t = [System.IO.File]::GetLastWriteTimeUtc($f)
        if ($t -gt $newestUtc) { $newestUtc = $t }
    }
}
$topCMakeLists = Join-Path $RepoDir 'CMakeLists.txt'
if (Test-Path $topCMakeLists) {
    $t = [System.IO.File]::GetLastWriteTimeUtc($topCMakeLists)
    if ($t -gt $newestUtc) { $newestUtc = $t }
}

if ($newestUtc -gt $exeTimeUtc) { exit 1 } else { exit 0 }

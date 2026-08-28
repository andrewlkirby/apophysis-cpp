# Installs the latest Apophysis 7X release for Windows - no Visual Studio,
# Qt SDK, or vcpkg required, since it downloads the prebuilt zip from
# GitHub Releases (built by .github/workflows/release.yml) rather than
# building from source. See README.md's "Building from source" section if
# you want to build it yourself instead.
#
# Usage (PowerShell):
#   irm https://raw.githubusercontent.com/andrewlkirby/apophysis-cpp/main/install.ps1 | iex
#
# By default (no switches, run interactively) this asks where you want a
# shortcut - Start Menu, Desktop, both, or neither (just unpacked into
# $InstallDir, for someone who'll launch it some other way, e.g. a script
# or a PATH entry they manage themselves). Piping into `iex` runs this as a
# script block with no param() binding of its own, so switches can't be
# tacked on after `iex` the usual way - use the `& { ... } -Args` form
# instead:
#   iex "& { $(irm https://raw.githubusercontent.com/andrewlkirby/apophysis-cpp/main/install.ps1) } -DesktopShortcut"
# Non-interactive sessions (CI, a redirected/piped stdin) skip the prompt
# and fall back to the old default (Start Menu only) unless a switch says
# otherwise, so existing automation using the plain `irm | iex` one-liner
# keeps working unchanged.

param(
    [switch]$StartMenuShortcut,
    [switch]$DesktopShortcut,
    [switch]$NoShortcut
)

$ErrorActionPreference = 'Stop'

$Repo = 'andrewlkirby/apophysis-cpp'
$ApiUrl = "https://api.github.com/repos/$Repo/releases/latest"
$InstallDir = Join-Path $env:LOCALAPPDATA 'Apophysis7X'

function Write-Step($Message) {
    Write-Host "==> $Message"
}

# Explicit switches always win (and work the same whether or not the
# session is interactive - the point of offering them at all is scriptable,
# unattended control). Only prompt when the caller didn't say anything.
if (-not ($StartMenuShortcut -or $DesktopShortcut -or $NoShortcut)) {
    if ([Console]::IsInputRedirected) {
        # Non-interactive (CI, piped stdin, ...) - Read-Host would hang
        # forever waiting on input that's never coming. Keep this script's
        # long-standing default (Start Menu shortcut only) so any existing
        # automation built around the plain `irm | iex` one-liner doesn't
        # silently change behavior.
        $StartMenuShortcut = $true
    } else {
        Write-Host ''
        Write-Host 'Where should a shortcut to Apophysis 7X go?'
        Write-Host '  [1] Start Menu (default)'
        Write-Host '  [2] Desktop'
        Write-Host '  [3] Both'
        Write-Host "  [4] Neither - just install to $InstallDir"
        $Choice = Read-Host 'Choose 1-4'
        switch ($Choice) {
            '2' { $DesktopShortcut = $true }
            '3' { $StartMenuShortcut = $true; $DesktopShortcut = $true }
            '4' { $NoShortcut = $true }
            default { $StartMenuShortcut = $true } # '1', blank, or anything unrecognized
        }
    }
}

Write-Step 'Looking up the latest release...'
try {
    $Release = Invoke-RestMethod -Uri $ApiUrl -UseBasicParsing
} catch {
    throw "couldn't reach $ApiUrl - check your internet connection. ($($_.Exception.Message))"
}

$ZipAsset = $Release.assets | Where-Object { $_.name -like 'apophysis7x-*-win64.zip' } | Select-Object -First 1
if (-not $ZipAsset) {
    throw "couldn't find a win64.zip release asset - see https://github.com/$Repo/releases for available downloads."
}
$SumsAsset = $Release.assets | Where-Object { $_.name -eq 'SHA256SUMS' } | Select-Object -First 1

$WorkDir = Join-Path ([System.IO.Path]::GetTempPath()) ([System.IO.Path]::GetRandomFileName())
New-Item -ItemType Directory -Path $WorkDir -Force | Out-Null
try {
    $ZipPath = Join-Path $WorkDir $ZipAsset.name
    Write-Step "Downloading $($ZipAsset.name)..."
    Invoke-WebRequest -Uri $ZipAsset.browser_download_url -OutFile $ZipPath -UseBasicParsing

    if ($SumsAsset) {
        Write-Step 'Verifying checksum...'
        $SumsPath = Join-Path $WorkDir 'SHA256SUMS'
        Invoke-WebRequest -Uri $SumsAsset.browser_download_url -OutFile $SumsPath -UseBasicParsing
        $ExpectedLine = Select-String -Path $SumsPath -Pattern ([regex]::Escape($ZipAsset.name)) | Select-Object -First 1
        if ($ExpectedLine) {
            $Expected = ($ExpectedLine.Line -split '\s+')[0].ToLower()
            $Actual = (Get-FileHash -Path $ZipPath -Algorithm SHA256).Hash.ToLower()
            if ($Expected -ne $Actual) {
                throw "checksum mismatch for $($ZipAsset.name) - download may be corrupted or tampered with. Expected $Expected, got $Actual."
            }
        }
    }

    Write-Step 'Extracting...'
    $ExtractDir = Join-Path $WorkDir 'extracted'
    Expand-Archive -Path $ZipPath -DestinationPath $ExtractDir -Force

    if (Test-Path $InstallDir) {
        Remove-Item -Path $InstallDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path (Split-Path $InstallDir -Parent) -Force | Out-Null
    Move-Item -Path $ExtractDir -Destination $InstallDir

    $ExePath = Join-Path $InstallDir 'apo_gui.exe'
    if (-not (Test-Path $ExePath)) {
        throw "apo_gui.exe not found in the extracted package at $InstallDir."
    }

    $ShortcutsMade = @()
    if ($StartMenuShortcut) {
        Write-Step 'Creating Start Menu shortcut...'
        $StartMenuDir = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs'
        $StartMenuShortcutPath = Join-Path $StartMenuDir 'Apophysis 7X.lnk'
        $WshShell = New-Object -ComObject WScript.Shell
        $Shortcut = $WshShell.CreateShortcut($StartMenuShortcutPath)
        $Shortcut.TargetPath = $ExePath
        $Shortcut.WorkingDirectory = $InstallDir
        $Shortcut.Save()
        $ShortcutsMade += 'the Start Menu (Apophysis 7X)'
    }
    if ($DesktopShortcut) {
        Write-Step 'Creating Desktop shortcut...'
        $DesktopShortcutPath = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Apophysis 7X.lnk'
        $WshShell = New-Object -ComObject WScript.Shell
        $Shortcut = $WshShell.CreateShortcut($DesktopShortcutPath)
        $Shortcut.TargetPath = $ExePath
        $Shortcut.WorkingDirectory = $InstallDir
        $Shortcut.Save()
        $ShortcutsMade += 'the Desktop'
    }

    Write-Step "Installed to $InstallDir"
    if ($ShortcutsMade) {
        Write-Host "Launch it from $($ShortcutsMade -join ' or ') or run:`n  $ExePath"
    } else {
        Write-Host "No shortcut created (as requested) - run it directly:`n  $ExePath"
    }
} finally {
    Remove-Item -Path $WorkDir -Recurse -Force -ErrorAction SilentlyContinue
}

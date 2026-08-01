# Installs the latest Apophysis 7X release for Windows - no Visual Studio,
# Qt SDK, or vcpkg required, since it downloads the prebuilt zip from
# GitHub Releases (built by .github/workflows/release.yml) rather than
# building from source. See README.md's "Building from source" section if
# you want to build it yourself instead.
#
# Usage (PowerShell):
#   irm https://raw.githubusercontent.com/andrewlkirby/apophysis-cpp/main/install.ps1 | iex

$ErrorActionPreference = 'Stop'

$Repo = 'andrewlkirby/apophysis-cpp'
$ApiUrl = "https://api.github.com/repos/$Repo/releases/latest"
$InstallDir = Join-Path $env:LOCALAPPDATA 'Apophysis7X'

function Write-Step($Message) {
    Write-Host "==> $Message"
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

    Write-Step 'Creating Start Menu shortcut...'
    $StartMenuDir = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs'
    $ShortcutPath = Join-Path $StartMenuDir 'Apophysis 7X.lnk'
    $WshShell = New-Object -ComObject WScript.Shell
    $Shortcut = $WshShell.CreateShortcut($ShortcutPath)
    $Shortcut.TargetPath = $ExePath
    $Shortcut.WorkingDirectory = $InstallDir
    $Shortcut.Save()

    Write-Step "Installed to $InstallDir"
    Write-Host "Launch it from the Start Menu (Apophysis 7X) or run:`n  $ExePath"
} finally {
    Remove-Item -Path $WorkDir -Recurse -Force -ErrorAction SilentlyContinue
}

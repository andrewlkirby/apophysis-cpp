; Inno Setup script for Apophysis 7X (C++ port) - Phase 8 (Packaging).
;
; Prerequisite: `cmake --build build --config Release --target deploy` (see
; cmake/Packaging.cmake) - this script packages that already-staged,
; self-contained build/deploy/ directory (apo_gui.exe, its Qt DLLs, MSVC
; redistributable CRT DLLs, and vcpkg deps), it doesn't build anything
; itself. Compile with: iscc packaging\apophysis7x.iss
; (run from the repo root, so the relative SourceDir path below resolves).
;
; No vc_redist.x64.exe prerequisite step is needed: build/deploy/ already
; carries its own copies of the MSVC CRT DLLs (vcruntime140.dll etc.) next
; to apo_gui.exe, so the app runs standalone regardless of what's already
; on the target machine - see Packaging.cmake's comment on why that
; matters (a packaged build with no CRT deployed crashes inside Qt6Core.dll
; with a raw buffer-overrun exception rather than a clean error, verified
; directly while setting this up).

#define AppName "Apophysis 7X"
#define AppVersion "1.0.0"
#define AppPublisher "Apophysis Developers Team"
#define AppExeName "apo_gui.exe"
#define SourceDir "..\build\deploy"

[Setup]
AppId={{B4A6F1E1-4B0E-4A6B-9A7C-0B7C8B4A6F1E}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\{#AppExeName}
OutputDir=..\build
OutputBaseFilename=apophysis7x-{#AppVersion}-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\LICENSE
; This port targets Windows only (see the migration plan's Scope section) -
; ARM64 is left for a future cross-compile, not attempted here.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked
; Optional, off by default - the original associated .flame/.flam3 with
; Apophysis, but silently taking over a file association during install
; (rather than asking) is exactly the kind of installer behavior users
; reasonably distrust, so this is opt-in.
Name: "fileassoc"; Description: "Open .flame and .flam3 files with {#AppName}"; GroupDescription: "File associations:"; Flags: unchecked

[Files]
; Recurse: pulls in windeployqt's plugin subdirectories (platforms/,
; styles/, imageformats/, ...) alongside the top-level DLLs/exe.
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.flame"; ValueType: string; ValueName: ""; ValueData: "Apophysis7X.Flame"; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\.flam3"; ValueType: string; ValueName: ""; ValueData: "Apophysis7X.Flame"; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\Apophysis7X.Flame"; ValueType: string; ValueName: ""; ValueData: "Apophysis Flame"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\Apophysis7X.Flame\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExeName},0"; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\Apophysis7X.Flame\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExeName}"" ""%1"""; Tasks: fileassoc

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent

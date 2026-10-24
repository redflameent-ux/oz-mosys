; Inno Setup script - builds Fourfold-Setup.exe
[Setup]
AppName=Fourfold
AppVersion=0.1.0
AppPublisher=Fourfold Audio
DefaultDirName={autopf}\Fourfold
DefaultGroupName=Fourfold
OutputBaseFilename=Fourfold-Setup
OutputDir=Output
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
DisableProgramGroupPage=yes
WizardStyle=modern

[Files]
; The VST3 plugin goes where every DAW (including FL Studio) looks for it
Source: "..\build\Fourfold_artefacts\Release\VST3\Fourfold.vst3\*"; DestDir: "{commoncf64}\VST3\Fourfold.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
; Stand-alone app for playing it without a DAW
Source: "..\build\Fourfold_artefacts\Release\Standalone\Fourfold.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Fourfold (stand-alone)"; Filename: "{app}\Fourfold.exe"

[Run]
Filename: "{app}\Fourfold.exe"; Description: "Try Fourfold now (stand-alone)"; Flags: nowait postinstall skipifsilent unchecked

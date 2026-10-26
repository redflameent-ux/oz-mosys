; Inno Setup script - builds VIBRA-VII-Setup.exe
[Setup]
AppName=VIBRA-VII ChromaBed
AppVersion=0.13.0
AppPublisher=TRANCE-6
DefaultDirName={autopf}\VIBRA-VII ChromaBed
DefaultGroupName=VIBRA-VII ChromaBed
OutputBaseFilename=VIBRA-VII-Setup
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
Source: "..\build\VibraVII_artefacts\Release\VST3\VIBRA-VII ChromaBed.vst3\*"; DestDir: "{commoncf64}\VST3\VIBRA-VII ChromaBed.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
; Stand-alone app for playing it without a DAW
Source: "..\build\VibraVII_artefacts\Release\Standalone\VIBRA-VII ChromaBed.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\VIBRA-VII ChromaBed (stand-alone)"; Filename: "{app}\VIBRA-VII ChromaBed.exe"

[Run]
Filename: "{app}\VIBRA-VII ChromaBed.exe"; Description: "Try VIBRA-VII ChromaBed now (stand-alone)"; Flags: nowait postinstall skipifsilent unchecked

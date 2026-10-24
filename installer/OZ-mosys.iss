; Inno Setup script - builds OZ-mosys-Setup.exe
[Setup]
AppName=OZ-mosys
AppVersion=0.12.0
AppPublisher=OZ-mosys
DefaultDirName={autopf}\OZ-mosys
DefaultGroupName=OZ-mosys
OutputBaseFilename=OZ-mosys-Setup
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
Source: "..\build\OZmosys_artefacts\Release\VST3\OZ-mosys.vst3\*"; DestDir: "{commoncf64}\VST3\OZ-mosys.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
; Stand-alone app for playing it without a DAW
Source: "..\build\OZmosys_artefacts\Release\Standalone\OZ-mosys.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\OZ-mosys (stand-alone)"; Filename: "{app}\OZ-mosys.exe"

[Run]
Filename: "{app}\OZ-mosys.exe"; Description: "Try OZ-mosys now (stand-alone)"; Flags: nowait postinstall skipifsilent unchecked

; Inno Setup script for the Windows installer.
; Build with: iscc /DAppVersion=1.1.0 /DSourceDir=<folder with VoxBloom.vst3 and VoxBloom.exe> /DOutputDir=dist VoxBloom.iss

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\build\VoxBloom_artefacts\Release"
#endif
#ifndef OutputDir
  #define OutputDir "..\dist"
#endif

[Setup]
AppId={{6F1C2A7E-3B5D-4E8A-9C21-7D4B0E5A9F13}
AppName=VoxBloom
AppVersion={#AppVersion}
AppPublisher=Drumkidbeatz
DefaultDirName={autopf}\Drumkidbeatz\VoxBloom
DefaultGroupName=VoxBloom
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=VoxBloom-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=VoxBloom

[Languages]
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Alles / Everything"
Name: "custom"; Description: "Auswahl / Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 Plugin"; Types: full custom; Flags: fixed
Name: "app"; Description: "Standalone App"; Types: full

[Files]
Source: "{#SourceDir}\VST3\VoxBloom.vst3\*"; DestDir: "{commoncf64}\VST3\VoxBloom.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\Standalone\VoxBloom.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion

[Icons]
Name: "{group}\VoxBloom"; Filename: "{app}\VoxBloom.exe"; Components: app
Name: "{group}\{cm:UninstallProgram,VoxBloom}"; Filename: "{uninstallexe}"

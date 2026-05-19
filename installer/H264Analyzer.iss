#define AppName "H264Analyzer"
#ifndef AppVersion
#define AppVersion "0.1.0"
#endif
#ifndef SourceDir
#define SourceDir "..\dist\H264Analyzer-windows-ucrt64"
#endif
#ifndef OutputDir
#define OutputDir "..\dist"
#endif
#ifndef OutputBaseFilename
#define OutputBaseFilename "H264Analyzer-windows-ucrt64-setup"
#endif

[Setup]
AppId={{7C6C1A56-6D79-4E42-9674-9D8D21691255}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=H264Analyzer
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBaseFilename}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\H264Analyzer.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\H264Analyzer"; Filename: "{app}\H264Analyzer.exe"; WorkingDir: "{app}"
Name: "{group}\Uninstall H264Analyzer"; Filename: "{uninstallexe}"
Name: "{autodesktop}\H264Analyzer"; Filename: "{app}\H264Analyzer.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\H264Analyzer.exe"; Description: "{cm:LaunchProgram,H264Analyzer}"; Flags: nowait postinstall skipifsilent unchecked

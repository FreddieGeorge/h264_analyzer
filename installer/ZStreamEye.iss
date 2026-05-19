#define AppName "ZStreamEye"
#ifndef AppVersion
#define AppVersion "0.1.7"
#endif
#ifndef SourceDir
#define SourceDir "..\dist\ZStreamEye-windows-ucrt64"
#endif
#ifndef OutputDir
#define OutputDir "..\dist"
#endif
#ifndef OutputBaseFilename
#define OutputBaseFilename "ZStreamEye-windows-ucrt64-setup"
#endif

[Setup]
AppId={{7C6C1A56-6D79-4E42-9674-9D8D21691255}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=ZStreamEye
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
UninstallDisplayIcon={app}\ZStreamEye.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[InstallDelete]
Type: files; Name: "{app}\*.dll"
Type: filesandordirs; Name: "{app}\bin"
Type: filesandordirs; Name: "{app}\generic"
Type: filesandordirs; Name: "{app}\imageformats"
Type: filesandordirs; Name: "{app}\networkinformation"
Type: filesandordirs; Name: "{app}\platforms"
Type: filesandordirs; Name: "{app}\styles"
Type: filesandordirs; Name: "{app}\tls"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\ZStreamEye"; Filename: "{app}\ZStreamEye.exe"; WorkingDir: "{app}"
Name: "{group}\Uninstall ZStreamEye"; Filename: "{uninstallexe}"
Name: "{autodesktop}\ZStreamEye"; Filename: "{app}\ZStreamEye.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\ZStreamEye.exe"; Description: "{cm:LaunchProgram,ZStreamEye}"; Flags: nowait postinstall skipifsilent unchecked

; Haigaku Manager Inno Setup Script
; Make sure Inno Setup is installed: https://jrsoftware.org/isinfo.php

#define MyAppName "Haigaku Manager"
#define MyAppVersion "1.0.0" ;
#define MyAppPublisher "Ketengan Diffusion™"
#define MyAppURL "https://github.com/Ketengan-Diffusion/HaigakuManager" ;
#define MyAppExeName "HaigakuManager.exe"
#define MySourcePath "build_release/Release" ;

[Setup]
AppId={{AUTO_GUID}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputBaseFilename=HaigakuManager_v{#MyAppVersion}_setup
OutputDir=installer_output ; Creates installer in 'installer_output' subdirectory
Compression=lzma
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
SetupIconFile=

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#MySourcePath}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourcePath}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; This line copies all files and subdirectories from your windeployqt output folder
; to the installation directory ({app}).

; NOTE: If you have very large DLLs or assets that don't change often,
; you might consider splitting them into components or separate downloads,
; but for most apps, including everything is fine.

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

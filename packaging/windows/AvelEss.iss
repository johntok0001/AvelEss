; Inno Setup script for Avel Ess (Windows, VST3).
; Build:  iscc /DAppVersion=1.0.0 packaging\windows\AvelEss.iss
; Expects the plugin at build\AvelEss_artefacts\Release\VST3\Avel Ess.vst3

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

#define Root "..\.."

[Setup]
AppId={{BB8F25F4-131C-437D-9F65-14301242198F}
AppName=Avel Ess
AppVersion={#AppVersion}
AppVerName=Avel Ess {#AppVersion}
AppPublisher=Avel
DefaultDirName={commoncf64}\VST3\Avel Ess.vst3
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
UninstallFilesDir={autopf}\Avel\Avel Ess
UninstallDisplayName=Avel Ess {#AppVersion} (VST3)
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#Root}\dist
OutputBaseFilename=AvelEss-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=no

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"

[Messages]
en.WelcomeLabel2=This will install Avel Ess {#AppVersion} (VST3) into%n%nC:\Program Files\Common Files\VST3%n%nClose your DAW before continuing.
ru.WelcomeLabel2=Будет установлен Avel Ess {#AppVersion} (VST3) в папку%n%nC:\Program Files\Common Files\VST3%n%nПеред продолжением закройте DAW.
en.FinishedLabel=Avel Ess is installed. Start your DAW and rescan plugins (REAPER: Preferences > Plug-ins > VST > Re-scan).
ru.FinishedLabel=Avel Ess установлен. Запустите DAW и пересканируйте плагины (REAPER: Preferences > Plug-ins > VST > Re-scan).

[InstallDelete]
; remove files of an older version inside the bundle
Type: filesandordirs; Name: "{commoncf64}\VST3\Avel Ess.vst3"

[Files]
Source: "{#Root}\build\AvelEss_artefacts\Release\VST3\Avel Ess.vst3\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Root}\README.md"; DestDir: "{autopf}\Avel\Avel Ess"; DestName: "README.txt"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Avel Ess.vst3"

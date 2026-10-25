; Windows installer for The Antiparticle (Inno Setup 6). Built by .github/workflows/release.yml, or locally after a
; Release build of TheAntiparticle_VST3, TheAntiparticle_CLAP and TheAntiparticle_Standalone:
;   iscc /DAppVersion=<version> packaging\TheAntiparticle.iss          (writes dist\TheAntiparticle-<version>-Windows-Setup.exe)
; /DTestRoot=<folder> makes a test installer that puts everything under that folder and needs no administrator rights
; (to check the packaging and the uninstaller without touching Program Files).

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifdef TestRoot
  #define VstDir TestRoot + "\VST3"
  #define ClapDir TestRoot + "\CLAP"
  #define AppDir TestRoot + "\App"
#else
  #define VstDir "{commoncf64}\VST3"
  #define ClapDir "{commoncf64}\CLAP"
  #define AppDir "{autopf}\The Antiparticle"
#endif
#ifndef Artefacts
  #define Artefacts "..\build\TheAntiparticle_artefacts\Release"
#endif

[Setup]
AppName=The Antiparticle
AppVersion={#AppVersion}
AppPublisher=The Antiparticle
AppPublisherURL=https://github.com/MaymiPR/TheAntiparticle
AppSupportURL=https://github.com/MaymiPR/TheAntiparticle/issues
DefaultDirName={#AppDir}
DefaultGroupName=The Antiparticle
DisableProgramGroupPage=yes
DisableDirPage=yes
LicenseFile=..\LICENSE
OutputDir=..\dist
#ifdef TestRoot
OutputBaseFilename=TheAntiparticle-{#AppVersion}-Windows-Setup-TEST
PrivilegesRequired=lowest
AppId={{099B530E-66B2-475A-8A2E-248EF6AF7479}
#else
OutputBaseFilename=TheAntiparticle-{#AppVersion}-Windows-Setup
PrivilegesRequired=admin
AppId={{099B530E-66B2-475A-8A2E-248EF6AF7478}
#endif
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=The Antiparticle
CloseApplications=yes

[Types]
Name: "full"; Description: "VST3 and CLAP plug-ins"
Name: "all"; Description: "VST3, CLAP and the standalone app"
Name: "custom"; Description: "Choose formats"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (C:\Program Files\Common Files\VST3)"; Types: full all custom
Name: "clap"; Description: "CLAP plug-in (C:\Program Files\Common Files\CLAP)"; Types: full all custom
Name: "standalone"; Description: "Standalone app (C:\Program Files\The Antiparticle)"; Types: all

[Files]
Source: "{#Artefacts}\VST3\The Antiparticle.vst3\*"; DestDir: "{#VstDir}\The Antiparticle.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Artefacts}\CLAP\The Antiparticle.clap"; DestDir: "{#ClapDir}"; Components: clap; Flags: ignoreversion
Source: "{#Artefacts}\Standalone\The Antiparticle.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\resources\fonts\OFL-InstrumentSans.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\The Antiparticle"; Filename: "{app}\The Antiparticle.exe"; Components: standalone
Name: "{group}\Uninstall The Antiparticle"; Filename: "{uninstallexe}"

[UninstallDelete]
Type: filesandordirs; Name: "{#VstDir}\The Antiparticle.vst3"
Type: files; Name: "{#ClapDir}\The Antiparticle.clap"

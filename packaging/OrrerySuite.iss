; Windows installer for the whole Orrery Suite (Inno Setup 7). Built by .github/workflows/release.yml, or locally after a
; Release build of every plug-in's _VST3, _CLAP and _Standalone targets:
;   pwsh packaging/make_iss_include.ps1 build      (writes packaging/plugins.iss.inc from plugins.txt)
;   iscc /DAppVersion=<version> packaging\OrrerySuite.iss          (writes dist\OrrerySuite-<version>-Windows-Setup.exe)
; /DTestRoot=<folder> makes a test installer that puts everything under that folder and needs no administrator rights
; (to check the packaging and the uninstaller without touching Program Files).

#ifndef AppVersion
  #define AppVersion "1.1.0"
#endif
#ifdef TestRoot
  #define VstDir TestRoot + "\VST3\Orrery Suite"
  #define ClapDir TestRoot + "\CLAP\Orrery Suite"
  #define AppDir TestRoot + "\App"
#else
  #define VstDir "{commoncf64}\VST3\Orrery Suite"
  #define ClapDir "{commoncf64}\CLAP\Orrery Suite"
  #define AppDir "{autopf}\Orrery Suite"
#endif

[Setup]
AppName=Orrery Suite
AppVersion={#AppVersion}
AppPublisher=Orrery Suite
AppPublisherURL=https://github.com/MaymiPR/Orrery-Suite
AppSupportURL=https://github.com/MaymiPR/Orrery-Suite/issues
DefaultDirName={#AppDir}
DefaultGroupName=Orrery Suite
DisableProgramGroupPage=yes
DisableDirPage=yes
LicenseFile=..\LICENSE
OutputDir=..\dist
#ifdef TestRoot
OutputBaseFilename=OrrerySuite-{#AppVersion}-Windows-Setup-TEST
PrivilegesRequired=lowest
AppId={{CFCB62BB-FB41-4B4B-AB2C-4ED7523FCF10}
#else
OutputBaseFilename=OrrerySuite-{#AppVersion}-Windows-Setup
PrivilegesRequired=admin
AppId={{CFCB62BB-FB41-4B4B-AB2C-4ED7523FCF0F}
#endif
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Inno Setup 7: a 64-bit Setup.exe (the default would still be 32-bit); AppVerName and TimeStampsInUTC are
; stated because their defaults changed in 7 (name plus version with no localized word; UTC time stamps).
SetupArchitecture=x64
AppVerName=Orrery Suite {#AppVersion}
TimeStampsInUTC=yes
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Orrery Suite
CloseApplications=yes

[Types]
Name: "full"; Description: "All plug-ins, VST3 and CLAP"
Name: "all"; Description: "All plug-ins, VST3, CLAP and the standalone apps"
Name: "custom"; Description: "Choose plug-ins and formats"; Flags: iscustom

; [Components], [Files], [Icons] and [UninstallDelete] for every plug-in in plugins.txt
#include "plugins.iss.inc"

[Files]
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "THIRD-PARTY-NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Uninstall Orrery Suite"; Filename: "{uninstallexe}"

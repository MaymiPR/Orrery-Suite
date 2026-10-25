; Windows installer for the whole Newton Suite (Inno Setup 6). Built by .github/workflows/release.yml, or locally after a
; Release build of every plug-in's _VST3, _CLAP and _Standalone targets:
;   pwsh packaging/make_iss_include.ps1 build      (writes packaging/plugins.iss.inc from plugins.txt)
;   iscc /DAppVersion=<version> packaging\NewtonSuite.iss          (writes dist\NewtonSuite-<version>-Windows-Setup.exe)
; /DTestRoot=<folder> makes a test installer that puts everything under that folder and needs no administrator rights
; (to check the packaging and the uninstaller without touching Program Files).

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifdef TestRoot
  #define VstDir TestRoot + "\VST3\Newton Suite"
  #define ClapDir TestRoot + "\CLAP\Newton Suite"
  #define AppDir TestRoot + "\App"
#else
  #define VstDir "{commoncf64}\VST3\Newton Suite"
  #define ClapDir "{commoncf64}\CLAP\Newton Suite"
  #define AppDir "{autopf}\Newton Suite"
#endif

[Setup]
AppName=Newton Suite
AppVersion={#AppVersion}
AppPublisher=Newton Suite
AppPublisherURL=https://github.com/MaymiPR/NewtonSuite
AppSupportURL=https://github.com/MaymiPR/NewtonSuite/issues
DefaultDirName={#AppDir}
DefaultGroupName=Newton Suite
DisableProgramGroupPage=yes
DisableDirPage=yes
LicenseFile=..\LICENSE
OutputDir=..\dist
#ifdef TestRoot
OutputBaseFilename=NewtonSuite-{#AppVersion}-Windows-Setup-TEST
PrivilegesRequired=lowest
AppId={{CFCB62BB-FB41-4B4B-AB2C-4ED7523FCF10}
#else
OutputBaseFilename=NewtonSuite-{#AppVersion}-Windows-Setup
PrivilegesRequired=admin
AppId={{CFCB62BB-FB41-4B4B-AB2C-4ED7523FCF0F}
#endif
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Newton Suite
CloseApplications=yes

[Types]
Name: "full"; Description: "All plug-ins, VST3 and CLAP"
Name: "all"; Description: "All plug-ins, VST3, CLAP and the standalone apps"
Name: "custom"; Description: "Choose plug-ins and formats"; Flags: iscustom

; [Components], [Files], [Icons] and [UninstallDelete] for every plug-in in plugins.txt
#include "plugins.iss.inc"

[Files]
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\plugins\TheAntiparticle\resources\fonts\OFL-InstrumentSans.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Uninstall Newton Suite"; Filename: "{uninstallexe}"

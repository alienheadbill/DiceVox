; DiceVox Windows installer (Inno Setup 6). Built by CI:
;   ISCC /DSourceDir=<staged package dir> /DAppVersion=0.1.0 /DOutputDir=<dir> installer\DiceVox.iss
;
; Installs DiceVox.vst3 into the standard VST3 folder (C:\Program Files\Common
; Files\VST3), which FL Studio and every other VST3 host scans, replacing any
; older DiceVox there. The uninstaller and the font licence texts go to
; C:\Program Files\AHE Audio\DiceVox, so nothing but the plug-in lands in the
; VST3 folder.

#ifndef SourceDir
  #define SourceDir "..\package"
#endif
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef OutputDir
  #define OutputDir "..\installer-out"
#endif

[Setup]
AppId={{6F1E4C2A-9B37-4D8E-A5C1-2D7F0B3E8A91}
AppName=DiceVox
AppVersion={#AppVersion}
AppVerName=DiceVox {#AppVersion}
AppPublisher=AHE Audio
DefaultDirName={commonpf64}\AHE Audio\DiceVox
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayName=DiceVox (AHE Audio)
OutputDir={#OutputDir}
OutputBaseFilename=DiceVox-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no

[InstallDelete]
; Start from a clean bundle so files from an older build never linger.
Type: filesandordirs; Name: "{commoncf64}\VST3\DiceVox.vst3"

[Files]
Source: "{#SourceDir}\DiceVox.vst3\*"; DestDir: "{commoncf64}\VST3\DiceVox.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion recursesubdirs createallsubdirs

[Messages]
FinishedLabel=DiceVox is installed in C:\Program Files\Common Files\VST3.%n%nIn FL Studio: Options > Manage plugins > Find installed plugins. DiceVox shows up under Effects (vendor AHE).

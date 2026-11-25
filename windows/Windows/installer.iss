#ifndef Version
  #define Version "2.1.1-alpha"
#endif
#ifndef BundleDir
  #error BundleDir is required
#endif
#ifndef OutputFolder
  #error OutputFolder is required
#endif

[Setup]
AppName=Skeuomorphic Space Panner 2.1.1 Alpha
AppVersion={#Version}
AppPublisher=gett1
DefaultDirName={commoncf64}\VST3\Skeuomorphic Space Panner 2.1.1 Alpha
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputFolder}
OutputBaseFilename=Skeuomorphic_Space_Panner_Alpha_Win_Installer
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#BundleDir}\*"; DestDir: "{app}\Skeuomorphic Space Panner 2.1.1 Alpha.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\Manuali\Manuale_Utente.pdf"; DestDir: "{commondocs}\Gettone\Skeuomorphic Space Panner 2.1.1 Alpha"; DestName: "manuale_space_panner.pdf"; Flags: ignoreversion
Source: "..\Manuali\Installazione_Windows.pdf"; DestDir: "{commondocs}\Gettone\Skeuomorphic Space Panner 2.1.1 Alpha"; Flags: ignoreversion
Source: "..\LEGGIMI.txt"; DestDir: "{commondocs}\Gettone\Skeuomorphic Space Panner 2.1.1 Alpha"; DestName: "README.txt"; Flags: ignoreversion
Source: "..\Assets\OFL.txt"; DestDir: "{commondocs}\Gettone\Skeuomorphic Space Panner 2.1.1 Alpha"; DestName: "PixelifySans-OFL.txt"; Flags: ignoreversion

[Icons]
Name: "{commonprograms}\Skeuomorphic Space Panner 2.1.1 Alpha Manual"; Filename: "{commondocs}\Gettone\Skeuomorphic Space Panner 2.1.1 Alpha\manuale_space_panner.pdf"

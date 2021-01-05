; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

[Setup]
AppName=mtPaint
AppVerName=mtPaint %VERSION%
AppPublisher=Dmitry Groshev
AppPublisherURL=http://mtpaint.sourceforge.net/
AppSupportURL=http://mtpaint.sourceforge.net/
AppUpdatesURL=http://mtpaint.sourceforge.net/
DefaultDirName={pf}\mtPaint-%VERSION%
DefaultGroupName=mtPaint
AllowNoIcons=yes
LicenseFile=.\COPYING.txt
OutputDir=C:\
OutputBaseFilename=mtpaint-%VERSION%-w64-setup
;SetupIconFile=C:\Program Files\mtPaint\mtpaint.ico
Compression=lzma
SolidCompression=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
MinVersion=6.1
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "dscreenicon"; Description: "Create a desktop screenshot icon"; GroupDescription: "{cm:AdditionalIcons}"
;Name: "dhandbook"; Description: "Create a desktop Handbook icon"; GroupDescription: "{cm:AdditionalIcons}"
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "qlscreenicon"; Description: "Create a Quick Launch screenshot icon"; GroupDescription: "{cm:AdditionalIcons}"
;Name: "qlhandbook"; Description: "Create a Quick Launch Handbook icon"; GroupDescription: "{cm:AdditionalIcons}"
Name: "sendtoicon"; Description: "Create a Send To icon"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: ".\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[INI]
Filename: "{app}\mtpaint.url"; Section: "InternetShortcut"; Key: "URL"; String: "http://mtpaint.sourceforge.net/"

[Icons]
Name: "{group}\mtPaint"; Filename: "{app}\bin\mtpaint.exe"; IconFilename: "{app}\mtpaint.ico"; WorkingDir: "{app}\bin"
Name: "{group}\mtPaint Handbook"; Filename: "{app}\docs\index.html"; WorkingDir: "{app}\docs"
Name: "{group}\{cm:ProgramOnTheWeb,mtPaint}"; Filename: "{app}\mtpaint.url";
Name: "{group}\{cm:UninstallProgram,mtPaint}"; Filename: "{uninstallexe}";
Name: "{autodesktop}\mtPaint"; Filename: "{app}\bin\mtpaint.exe"; Tasks: desktopicon; IconFilename: "{app}\mtpaint.ico"; WorkingDir: "{app}\bin"
Name: "{autodesktop}\mtPaint Screenshot"; Filename: "{app}\bin\mtpaint.exe"; Parameters: "-s"; Tasks: dscreenicon; IconFilename: "{app}\screenshot.ico"; WorkingDir: "{app}\bin"
;Name: "{autodesktop}\mtPaint Handbook"; Filename: "{app}\docs\index.html"; Tasks: dhandbook; WorkingDir: "{app}\docs"
Name: "{autoappdata}\Microsoft\Internet Explorer\Quick Launch\mtPaint"; Filename: "{app}\bin\mtpaint.exe"; Tasks: quicklaunchicon; IconFilename: "{app}\mtpaint.ico"; WorkingDir: "{app}\bin"
Name: "{autoappdata}\Microsoft\Internet Explorer\Quick Launch\mtPaint Screenshot"; Filename: "{app}\bin\mtpaint.exe"; Parameters: "-s"; Tasks: qlscreenicon; IconFilename: "{app}\screenshot.ico"; WorkingDir: "{app}\bin"
;Name: "{autoappdata}\Microsoft\Internet Explorer\Quick Launch\mtPaint Handbook"; Filename: "{app}\docs\index.html"; Tasks: qlhandbook; WorkingDir: "{app}\docs"
Name: "{usersendto}\mtPaint"; Filename: "{app}\bin\mtpaint.exe"; Tasks: sendtoicon; IconFilename: "{app}\mtpaint.ico"; WorkingDir: "{app}\bin"

[Run]
Filename: "{app}\bin\mtpaint.exe"; Description: "{cm:LaunchProgram,mtPaint}"; Flags: nowait postinstall skipifsilent
Filename: "{app}\NEWS.txt"; Description: "View the NEWS file"; Flags: postinstall nowait shellexec skipifsilent
Filename: "{app}\README.txt"; Description: "View the README file"; Flags: postinstall nowait shellexec skipifsilent unchecked
;Filename: "{app}\CREDITS.txt"; Description: "View the CREDITS file"; Flags: postinstall nowait shellexec skipifsilent unchecked
Filename: "{app}\docs\index.html"; Description: "Read the mtPaint Handbook"; Flags: postinstall nowait shellexec skipifsilent unchecked

[UninstallDelete]
Type: files; Name: "{app}\mtpaint.url"


[Setup]
AppName=CobolWorks
AppVersion=1.0.0
AppPublisher=AnabasaSoft
; Se instalará en C:\Archivos de Programa\CobolWorks
DefaultDirName={autopf}\CobolWorks
DefaultGroupName=CobolWorks
DisableProgramGroupPage=yes
; La carpeta donde GitHub guardará el .exe final
OutputDir=Output
OutputBaseFilename=CobolWorks-Windows-Installer
Compression=lzma
SolidCompression=yes

[Tasks]
Name: "desktopicon"; Description: "Crear un acceso directo en el escritorio"; GroupDescription: "Accesos directos:"

[Files]
; Cogemos todo lo que preparamos en la carpeta Release-Windows y lo metemos al instalador
Source: "build\Release-Windows\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\CobolWorks"; Filename: "{app}\CobolWorks.exe"
Name: "{autodesktop}\CobolWorks"; Filename: "{app}\CobolWorks.exe"; Tasks: desktopicon

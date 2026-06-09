#define MyAppName "HasciiCam"
#define MyAppPublisher "Dyne.org foundation"
#define MyAppURL "https://dyne.org/"

#ifndef MyAppVersion
#error MyAppVersion is required
#endif

#ifndef MyBuildHome
#error MyBuildHome is required
#endif

#ifndef MyOutputDir
#error MyOutputDir is required
#endif

#ifndef HasSDL2Dll
#define HasSDL2Dll 0
#endif

[Setup]
AppId={{7C1D6B7A-0F0A-4A18-A7E5-6F2B8D2F6F3D}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
UninstallDisplayIcon={app}\hasciicam.exe
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
DisableWelcomePage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
DisableFinishedPage=no
DisableDirPage=auto
UsePreviousAppDir=yes
UsePreviousTasks=yes
WizardStyle=modern
LicenseFile={#MyBuildHome}\COPYING
InfoBeforeFile={#MyBuildHome}\docs\windows-installer.md
OutputBaseFilename=hasciicam-{#MyAppVersion}-windows-x64-setup
OutputDir={#MyOutputDir}
SolidCompression=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Full"
Name: "application"; Description: "Application only"

[Components]
Name: "main"; Description: "HasciiCam program, documentation, and shared runtime files"; Types: full application; Flags: fixed
Name: "virtualcamera"; Description: "Install and register the Windows virtual camera (Windows 11 build 22000+, administrator rights)"; Types: full

[Tasks]
Name: "addpath"; Description: "Add HasciiCam to PATH"

[Files]
Source: "{#MyBuildHome}\bin\hasciicam.exe"; DestDir: "{app}"; Components: main; Flags: ignoreversion
Source: "{#MyBuildHome}\bin\hasciicam_vcamctl.exe"; DestDir: "{app}"; Components: virtualcamera; Flags: ignoreversion
Source: "{#MyBuildHome}\bin\hasciicam_virtual_camera_source.dll"; DestDir: "{app}"; Components: virtualcamera; Flags: ignoreversion
#if HasSDL2Dll
Source: "{#MyBuildHome}\bin\SDL2.dll"; DestDir: "{app}"; Components: main; Flags: ignoreversion
#endif
Source: "{#MyBuildHome}\README.md"; DestDir: "{app}"; Components: main; Flags: ignoreversion
Source: "{#MyBuildHome}\docs\hasciicam.1"; DestDir: "{app}\docs"; Components: main; Flags: ignoreversion
Source: "{#MyBuildHome}\docs\windows-installer.md"; DestDir: "{app}\docs"; Components: main; Flags: ignoreversion
Source: "{#MyBuildHome}\COPYING"; DestDir: "{app}\licenses"; Components: main; Flags: ignoreversion
Source: "{#MyBuildHome}\licenses\*"; DestDir: "{app}\licenses"; Components: main; Flags: ignoreversion recursesubdirs createallsubdirs

[Run]
Filename: "{app}\hasciicam.exe"; Parameters: "-h"; Description: "Show HasciiCam help"; Flags: postinstall nowait skipifsilent unchecked

[Code]
const
  EnvironmentKey = 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';
  MyHWND_BROADCAST = $FFFF;
  MyWM_SETTINGCHANGE = $001A;
  MySMTO_ABORTIFHUNG = $0002;
  MySW_HIDE = 0;

function SendMessageTimeout(hWnd: Integer;
                            Msg: Integer;
                            wParam: Integer;
                            lParam: string;
                            fuFlags: Integer;
                            uTimeout: Integer;
                            var lpdwResult: Integer): Integer;
  external 'SendMessageTimeoutW@user32.dll stdcall';

function RunHelper(const Params: string; var ResultCode: Integer): Boolean;
begin
  Result := Exec(ExpandConstant('{app}\hasciicam_vcamctl.exe'),
                 Params,
                 ExpandConstant('{app}'),
                 MySW_HIDE,
                 ewWaitUntilTerminated,
                 ResultCode);
end;

function NormalizePathEntry(const Entry: string): string;
var
  Value: string;
begin
  Value := Trim(Entry);
  if (Length(Value) >= 2) and (Value[1] = '"') and (Value[Length(Value)] = '"') then
  begin
    Delete(Value, Length(Value), 1);
    Delete(Value, 1, 1);
    Value := Trim(Value);
  end;
  while (Length(Value) > 3) and ((Value[Length(Value)] = '\') or (Value[Length(Value)] = '/')) do
    Delete(Value, Length(Value), 1);
  Result := Lowercase(Value);
end;

function PathContainsEntry(const PathValue, Entry: string): Boolean;
var
  Remaining: string;
  Item: string;
  Delim: Integer;
  Wanted: string;
begin
  Result := False;
  Wanted := NormalizePathEntry(Entry);
  Remaining := PathValue;
  while Remaining <> '' do
  begin
    Delim := Pos(';', Remaining);
    if Delim = 0 then
    begin
      Item := Remaining;
      Remaining := '';
    end
    else
    begin
      Item := Copy(Remaining, 1, Delim - 1);
      Delete(Remaining, 1, Delim);
    end;
    if CompareText(NormalizePathEntry(Item), Wanted) = 0 then
    begin
      Result := True;
      Exit;
    end;
  end;
end;

function RebuildPathWithoutEntry(const PathValue, Entry: string; var NewPath: string): Boolean;
var
  Remaining: string;
  Item: string;
  Delim: Integer;
  Wanted: string;
  Normalized: string;
begin
  Result := False;
  NewPath := '';
  Wanted := NormalizePathEntry(Entry);
  Remaining := PathValue;
  while Remaining <> '' do
  begin
    Delim := Pos(';', Remaining);
    if Delim = 0 then
    begin
      Item := Remaining;
      Remaining := '';
    end
    else
    begin
      Item := Copy(Remaining, 1, Delim - 1);
      Delete(Remaining, 1, Delim);
    end;
    Normalized := NormalizePathEntry(Item);
    if (Normalized <> '') and (CompareText(Normalized, Wanted) <> 0) then
    begin
      if NewPath <> '' then
        NewPath := NewPath + ';';
      NewPath := NewPath + Trim(Item);
    end;
  end;
  Result := True;
end;

procedure BroadcastEnvironmentChange;
var
  ResultCode: Integer;
begin
  SendMessageTimeout(MyHWND_BROADCAST,
                     MyWM_SETTINGCHANGE,
                     0,
                     'Environment',
                     MySMTO_ABORTIFHUNG,
                     5000,
                     ResultCode);
end;

function UpdateMachinePath(AddEntry: Boolean): Boolean;
var
  CurrentPath: string;
  UpdatedPath: string;
  AppPath: string;
begin
  Result := False;
  AppPath := ExpandConstant('{app}');
  if not RegQueryStringValue(HKEY_LOCAL_MACHINE, EnvironmentKey, 'Path', CurrentPath) then
    CurrentPath := '';
  if AddEntry then
  begin
    if PathContainsEntry(CurrentPath, AppPath) then
    begin
      Result := True;
      Exit;
    end;
    if CurrentPath = '' then
      UpdatedPath := AppPath
    else
      UpdatedPath := CurrentPath + ';' + AppPath;
  end
  else
  begin
    if not RebuildPathWithoutEntry(CurrentPath, AppPath, UpdatedPath) then
      Exit;
    if CompareText(CurrentPath, UpdatedPath) = 0 then
    begin
      Result := True;
      Exit;
    end;
  end;
  if not RegWriteExpandStringValue(HKEY_LOCAL_MACHINE, EnvironmentKey, 'Path', UpdatedPath) then
    Exit;
  BroadcastEnvironmentChange;
  Result := True;
end;

function RemoveInstalledVirtualCamera: Boolean;
var
  ResultCode: Integer;
begin
  Result := True;
  if not FileExists(ExpandConstant('{app}\hasciicam_vcamctl.exe')) then
    Exit;
  if not RunHelper('remove --root "{app}"', ResultCode) then
  begin
    Result := False;
    Exit;
  end;
  if ResultCode <> 0 then
    Result := False;
end;

function InstallVirtualCamera: Boolean;
var
  ResultCode: Integer;
begin
  Result := True;
  if not WizardIsComponentSelected('virtualcamera') then
    Exit;
  if not RunHelper('install --source "{app}\hasciicam_virtual_camera_source.dll" --root "{app}"',
                   ResultCode) then
  begin
    Result := False;
    Exit;
  end;
  if ResultCode <> 0 then
    Result := False;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if not RemoveInstalledVirtualCamera then
    Result := 'Unable to remove the existing HasciiCam virtual camera before upgrading. Stop Windows Frame Server if it is holding the DLL and retry.';
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    if not InstallVirtualCamera then
      RaiseException('Unable to register the HasciiCam virtual camera. Stop Windows Frame Server if it is holding the DLL and retry.');
    if WizardIsTaskSelected('addpath') then
    begin
      if not UpdateMachinePath(True) then
        MsgBox('HasciiCam was installed, but PATH could not be updated automatically.', mbInformation, MB_OK);
    end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
  begin
    if not RemoveInstalledVirtualCamera then
      RaiseException('Unable to unregister the HasciiCam virtual camera. Stop Windows Frame Server if it is holding the DLL and retry.');
    if not UpdateMachinePath(False) then
      MsgBox('HasciiCam was removed, but PATH could not be updated automatically.', mbInformation, MB_OK);
  end;
end;

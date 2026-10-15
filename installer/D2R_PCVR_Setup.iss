; Native OpenXR installer. Build with build_native_installer.ps1.
#ifndef PayloadDir
  #error PayloadDir must point to a verified native Windows package with its private Python runtime.
#endif
#ifndef ModVersion
  #define ModVersion "0.1.2"
#endif
#ifndef InstallerOutput
  #define InstallerOutput "Output"
#endif

[Setup]
AppId=D2R-PCVR-Plus-Windows
AppName=D2R PCVR Plus
AppVersion={#ModVersion}
AppPublisher=D2R PCVR Plus contributors
AppPublisherURL=https://github.com/SarbaKhan/d2r-pcvr-plus
AppSupportURL=https://github.com/SarbaKhan/d2r-pcvr-plus/issues
DefaultDirName={localappdata}\Programs\D2R PCVR Plus
DefaultGroupName=D2R PCVR Plus
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableWelcomePage=no
DisableFinishedPage=no
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
LicenseFile={#PayloadDir}\LICENSE
OutputDir={#InstallerOutput}
OutputBaseFilename=D2R-PCVR-PLUS-Setup-{#ModVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=no
RestartApplications=no
UninstallDisplayName=D2R PCVR Plus (Windows Flat/VR mod)
UninstallDisplayIcon={app}\.runtime\python\python.exe

[Files]
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Installation help"; Filename: "{sys}\notepad.exe"; Parameters: """{app}\PCVR-PLUS-SETUP.md"""
Name: "{group}\Controller mappings"; Filename: "{sys}\notepad.exe"; Parameters: """{app}\CONTROLS.md"""
Name: "{group}\Uninstall D2R PCVR Plus"; Filename: "{uninstallexe}"

[Run]
Filename: "{code:SteamExecutable}"; Description: "Open Steam to use your new shortcuts"; Flags: nowait postinstall skipifsilent; Check: SetupSucceeded

[Code]
var
  EditionPage, HeadsetPage, ConnectionPage, AccountPage, ModePage, KeyboardPage, TextPage, AudioPage: TInputOptionWizardPage;
  PathsPage, RuntimePage, BackupPage: TInputDirWizardPage;
  ProfileIDs: TStringList;
  PayloadRoot, ProbeFile, ChoicesFile, LastOutput, PreviousEdition, BackupGame, PreferencesGame: String;
  SetupFailed, BackendBusy: Boolean;
  LocaleCodes: TArrayOfString;

function Q(const Value: String): String;
begin
  Result := '"' + Value + '"';
end;

procedure BackendOutput(const S: String; const Error, FirstLine: Boolean);
begin
  Log(S);
  if Trim(S) <> '' then LastOutput := S;
  if BackendBusy then begin
    WizardForm.StatusLabel.Caption := Copy(S, 1, 180);
    WizardForm.StatusLabel.Update;
    SaveStringToFile(ExpandConstant('{app}\setup-last.log'), S + #13#10, True);
  end;
end;

function RunBackend(const Root, Arguments: String): Boolean;
var
  ExitCode: Integer;
begin
  LastOutput := '';
  ExitCode := -1;
  Result := ExecAndLogOutput(Root + '\.runtime\python\python.exe',
    '-u ' + Q(Root + '\steamvr\setup_native.py') + ' ' + Arguments,
    Root, SW_SHOWNORMAL, ewWaitUntilTerminated, ExitCode, @BackendOutput);
  Result := Result and (ExitCode = 0);
  if not Result and (LastOutput = '') then
    LastOutput := 'Could not start the setup helper. Check Windows Security Protection History and keep your installation backups.';
end;

function InitializeSetup: Boolean;
begin
  Result := not CheckForMutexes('D2R-PCVR-Managed-Operation');
  if Result then CreateMutex('D2R-PCVR-Managed-Operation')
  else MsgBox('Another D2R PCVR Plus setup or removal is running. Close it first.', mbError, MB_OK);
  if WizardSilent and (ExpandConstant('{param:CONFIG|}') = '') and
      (ExpandConstant('{param:PAYLOADCHECK|}') = '') then begin
    Log('Silent setup requires /CONFIG="path to setup choices INI". No game files were changed.');
    Result := False;
  end;
end;

procedure LoadProfiles;
var
  I, Count, SelectedIndex: Integer;
  ID, Name, SelectedProfile: String;
begin
  SelectedProfile := '';
  if (AccountPage.SelectedValueIndex >= 0) and (AccountPage.SelectedValueIndex < ProfileIDs.Count) then
    SelectedProfile := ProfileIDs[AccountPage.SelectedValueIndex];
  SelectedIndex := 0;
  ProfileIDs.Clear;
  AccountPage.CheckListBox.Items.Clear;
  Count := GetIniInt('detected', 'profiles', 0, 0, 1000, ProbeFile);
  for I := 0 to Count - 1 do begin
    ID := GetIniString('detected', 'profile_' + IntToStr(I), '', ProbeFile);
    if ID = SelectedProfile then SelectedIndex := I;
    Name := GetIniString('detected', 'profile_name_' + IntToStr(I), 'Steam account', ProbeFile);
    ProfileIDs.Add(ID);
    AccountPage.Add(Name + ' (local ID ' + ID + ')');
  end;
  if Count > 0 then AccountPage.SelectedValueIndex := SelectedIndex;
end;

function RefreshProbe(const Steam: String): Boolean;
var
  Arguments, FoundBackup: String;
begin
  Arguments := '--probe ' + Q(ProbeFile) + ' --source-root ' + Q(ExpandConstant('{src}'));
  if Steam <> '' then begin
    Arguments := Arguments + ' --steam ' + Q(Steam) + ' --managed-root ' + Q(WizardDirValue);
    if CompareText(BackupGame, PathsPage.Values[1]) <> 0 then begin
      BackupPage.Values[0] := '';
      BackupGame := PathsPage.Values[1];
    end;
    if PathsPage.Values[1] <> '' then begin
      Arguments := Arguments + ' --game ' + Q(PathsPage.Values[1]);
      if BackupPage.Values[0] <> '' then Arguments := Arguments + ' --backup-hint ' + Q(BackupPage.Values[0]);
    end;
  end;
  Result := RunBackend(PayloadRoot, Arguments);
  if Result then begin
    LoadProfiles;
    FoundBackup := GetIniString('detected', 'backup_folder', '', ProbeFile);
    if (FoundBackup <> '') or (GetIniInt('detected', 'backup_required', 0, 0, 1, ProbeFile) = 0) then
      BackupPage.Values[0] := FoundBackup;
  end;
end;

procedure FillLanguages(Page: TInputOptionWizardPage);
begin
  Page.Add('Default (game language)');
  Page.Add('English'); Page.Add('German'); Page.Add('Spanish (Spain)');
  Page.Add('French'); Page.Add('Italian'); Page.Add('Korean'); Page.Add('Polish');
  Page.Add('Russian'); Page.Add('Chinese (Simplified)'); Page.Add('Chinese (Traditional)');
  Page.Add('Spanish (Latin America)'); Page.Add('Japanese'); Page.Add('Portuguese (Brazil)');
  Page.SelectedValueIndex := 0;
end;

procedure LoadPreferences;
var
  I: Integer;
begin
  KeyboardPage.SelectedValueIndex := 0;
  if GetIniString('detected', 'layout', 'WASD', ProbeFile) = 'ZQSD' then
    KeyboardPage.SelectedValueIndex := 1;
  TextPage.SelectedValueIndex := 0;
  AudioPage.SelectedValueIndex := 0;
  for I := 0 to GetArrayLength(LocaleCodes) - 1 do begin
    if LocaleCodes[I] = GetIniString('detected', 'text_locale', '', ProbeFile) then TextPage.SelectedValueIndex := I;
    if LocaleCodes[I] = GetIniString('detected', 'audio_locale', '', ProbeFile) then AudioPage.SelectedValueIndex := I;
  end;
end;

procedure InitializeWizard;
begin
  WizardForm.WelcomeLabel2.Caption :=
    'Play your own Diablo II: Resurrected in VR or on a flat screen.'#13#10#13#10 +
    'Supports SteamVR, Virtual Desktop and Flat Screen on Windows.'#13#10#13#10 +
    'You must purchase and install the original game. No Blizzard game binaries or archives are included. Library artwork is credited separately. Use an OFFLINE local character with this preview.'#13#10#13#10 +
    'Launch the original game once and complete its account prompts before installing. Save and exit D2R, then fully exit Steam before clicking Install.';
  PayloadRoot := ExpandConstant('{tmp}\') + '{app}';
  ProbeFile := ExpandConstant('{tmp}\detected.ini');
  ChoicesFile := ExpandConstant('{tmp}\choices.ini');
  ExtractTemporaryFiles('{app}\*');
  if ExpandConstant('{param:PAYLOADCHECK|}') <> '' then begin
    if not RunBackend(PayloadRoot, '--check') then RaiseException(LastOutput);
    Log('D2R PCVR Plus PAYLOADCHECK PASSED; no game or Steam files changed.');
    Abort;
  end;
  ProfileIDs := TStringList.Create;
  EditionPage := CreateInputOptionPage(wpLicense, 'Choose your game installation',
    'Steam or Battle.net', 'Select the copy you want to install PCVR Plus into. Battle.net support is experimental.', True, False);
  EditionPage.Add('Steam');
  HeadsetPage := CreateInputOptionPage(EditionPage.ID, 'Choose your headset',
    'Controller layout and connection', 'For VR sessions. Flat Screen does not require a headset. Quest 2 uses Quest controls.', True, False);
  HeadsetPage.Add('Steam Frame');
  HeadsetPage.Add('Quest 3');
  HeadsetPage.Add('Quest 2');
  HeadsetPage.Add('Other SteamVR headset');
  HeadsetPage.SelectedValueIndex := StrToIntDef(GetPreviousData('Headset', '0'), 0);
  ConnectionPage := CreateInputOptionPage(HeadsetPage.ID, 'Choose how to play',
    'Choose the connection you use', 'Virtual Desktop requires its headset app and Windows Streamer. Battle.net with VDXR has not yet been confirmed.', True, False);
  ConnectionPage.Add('Steam Link / SteamVR');
  ConnectionPage.Add('Virtual Desktop / VDXR');
  ConnectionPage.Add('Flat Screen (no VR)');
  ConnectionPage.Add('All (create three Steam library shortcuts)');
  ConnectionPage.SelectedValueIndex := StrToIntDef(GetPreviousData('Connection', '0'), 0);
  PathsPage := CreateInputDirPage(ConnectionPage.ID, 'Locate the installed game',
    'Detected folders can be changed with Browse', 'Select existing folders. Saved Games must contain Settings.json from the original game''s first launch.', False, '');
  PathsPage.Add('Steam folder (contains steam.exe):');
  PathsPage.Add('Diablo II: Resurrected folder (contains D2R.exe):');
  PathsPage.Add('Diablo II: Resurrected Saved Games folder:');
  RuntimePage := CreateInputDirPage(PathsPage.ID, 'Locate SteamVR', 'Choose its installed folder',
    'The SteamVR folder contains steamxr_win64.json. Install SteamVR through Steam if it is missing.', False, '');
  RuntimePage.Add('SteamVR folder:');
  AccountPage := CreateInputOptionPage(RuntimePage.ID, 'Choose your Steam account',
    'Steam library shortcuts', 'Selected shortcuts will be added to this local account. Other games and shortcuts are kept.', True, True);
  LocaleCodes := ['', 'enUS', 'deDE', 'esES', 'frFR', 'itIT', 'koKR', 'plPL', 'ruRU', 'zhCN', 'zhTW', 'esMX', 'jaJP', 'ptBR'];
  KeyboardPage := CreateInputOptionPage(AccountPage.ID, 'Choose your keyboard layout',
    'Flat Screen movement', 'This changes Flat Screen movement only. VR controls are preserved.', True, False);
  KeyboardPage.Add('QWERTY (WASD)');
  KeyboardPage.Add('AZERTY (ZQSD)');
  KeyboardPage.SelectedValueIndex := 0;
  TextPage := CreateInputOptionPage(KeyboardPage.ID, 'Choose the text language',
    'Game interface and subtitles', 'Default uses the game language configured in your launcher.', True, False);
  FillLanguages(TextPage);
  AudioPage := CreateInputOptionPage(TextPage.ID, 'Choose the audio language',
    'Spoken dialogue', 'Text and audio can use different languages. Default keeps the game language.', True, False);
  FillLanguages(AudioPage);
  ModePage := CreateInputOptionPage(AudioPage.ID, 'Install or update',
    'Choose how to apply settings', 'Game files, settings and Steam configuration are backed up. Character progress is kept.', True, False);
  ModePage.Add('Update / repair and keep calibration and video settings');
  ModePage.Add('Full install with recommended settings (resets calibration)');
  ModePage.Add('Only configure Steam shortcuts (keep the installed mod)');
  ModePage.SelectedValueIndex := 1;
  BackupPage := CreateInputDirPage(ModePage.ID, 'Locate an older installation backup',
    'Keep the original files available for removal',
    'Setup fills in a verified backup when it can find one. Otherwise select the original snapshot or its backup folder (usually setup-backups\game or build-frame\backups in the old mod folder). Setup will copy it into the helper folder and remember it for this game. If it is unavailable, cancel and keep the current installation.', False, '');
  BackupPage.Add('Original installation backup folder:');
  if not RefreshProbe('') then RaiseException(LastOutput);
  if GetIniInt('detected', 'battlenet_available', 0, 0, 1, ProbeFile) = 1 then
    EditionPage.Add('Battle.net (experimental)');
  EditionPage.SelectedValueIndex := 0;
  PathsPage.Values[0] := GetIniString('detected', 'steam', '', ProbeFile);
  PathsPage.Values[1] := GetIniString('detected', 'steam_game', '', ProbeFile);
  PathsPage.Values[2] := GetIniString('detected', 'saves', '', ProbeFile);
  RuntimePage.Values[0] := GetIniString('detected', 'steamvr', '', ProbeFile);
  PreviousEdition := 'steam';
  PreferencesGame := PathsPage.Values[1];
  LoadPreferences;
end;

function UsesSteamVR: Boolean;
begin
  Result := ConnectionPage.SelectedValueIndex in [0, 3];
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  if ExpandConstant('{param:CONFIG|}') <> '' then begin
    Result := (PageID >= EditionPage.ID) and (PageID <= BackupPage.ID);
    Exit;
  end;
  Result := ((PageID = RuntimePage.ID) and not UsesSteamVR) or
            ((PageID = AccountPage.ID) and (ProfileIDs.Count = 1)) or
            ((PageID = BackupPage.ID) and (GetIniInt('detected', 'backup_required', 0, 0, 1, ProbeFile) = 0));
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  Edition, Key: String;
begin
  Result := True;
  if ExpandConstant('{param:CONFIG|}') <> '' then Exit;
  if CurPageID = EditionPage.ID then begin
    Edition := 'steam';
    Key := 'steam_game';
    if EditionPage.SelectedValueIndex = 1 then begin Edition := 'battlenet'; Key := 'battlenet_game'; end;
    if Edition <> PreviousEdition then PathsPage.Values[1] := GetIniString('detected', Key, '', ProbeFile);
    PreviousEdition := Edition;
  end;
  if CurPageID = PathsPage.ID then begin
    if not FileExists(PathsPage.Values[0] + '\steam.exe') or
       not FileExists(PathsPage.Values[1] + '\D2R.exe') or
       not FileExists(PathsPage.Values[2] + '\Settings.json') then begin
      MsgBox('Select the existing Steam, game and Saved Games folders. Launch the original game once if Settings.json is missing.', mbError, MB_OK);
      Result := False;
    end else begin
      Result := RefreshProbe(PathsPage.Values[0]);
      if not Result then MsgBox(LastOutput, mbError, MB_OK);
      if ProfileIDs.Count = 0 then begin
        MsgBox('Sign into Steam once on this PC so setup can find your Steam account, then retry.', mbError, MB_OK);
        Result := False;
      end;
      if CompareText(PreferencesGame, PathsPage.Values[1]) <> 0 then begin
        LoadPreferences;
        PreferencesGame := PathsPage.Values[1];
      end;
      if RuntimePage.Values[0] = '' then RuntimePage.Values[0] := GetIniString('detected', 'steamvr', '', ProbeFile);
      if FileExists(PathsPage.Values[1] + '\d2rloader\plugins\d2rl-vrcam.dll') then ModePage.SelectedValueIndex := 0
      else ModePage.SelectedValueIndex := 1;
    end;
  end;
  if (CurPageID = ModePage.ID) and (ModePage.SelectedValueIndex = 1) and
      FileExists(PathsPage.Values[1] + '\d2rloader\plugins\d2rl-vrcam.dll') then
    Result := MsgBox('Full install resets body calibration and applies recommended video settings. Continue?', mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES;
  if CurPageID = BackupPage.ID then begin
    if not DirExists(BackupPage.Values[0]) then begin
      MsgBox('Select the existing original installation backup folder, or cancel to keep the current mod.', mbError, MB_OK);
      Result := False;
    end else begin
      Result := RefreshProbe(PathsPage.Values[0]);
      if not Result then MsgBox(LastOutput, mbError, MB_OK)
      else if (GetIniInt('detected', 'backup_required', 0, 0, 1, ProbeFile) = 1) and
          (GetIniString('detected', 'backup_folder', '', ProbeFile) = '') then begin
        MsgBox(GetIniString('detected', 'backup_message', 'The selected folder does not contain the original backup for this game.', ProbeFile), mbError, MB_OK);
        Result := False;
      end;
    end;
  end;
end;

procedure WriteChoices;
var
  Headsets, Connections, Modes, Layouts, Lines: TArrayOfString;
  Connection: String;
begin
  Headsets := ['Steam Frame', 'Quest 3', 'Quest 2', 'Other SteamVR headset'];
  Connections := ['steamvr', 'vdxr', 'flat', 'steamvr,vdxr,flat'];
  Modes := ['repair', 'full', 'shortcuts'];
  Layouts := ['WASD', 'ZQSD'];
  Connection := Connections[ConnectionPage.SelectedValueIndex];
  Lines := ['[setup]', 'store=' + PreviousEdition,
    'headset=' + Headsets[HeadsetPage.SelectedValueIndex], 'connections=' + Connection,
    'steam=' + PathsPage.Values[0], 'game=' + PathsPage.Values[1], 'saves=' + PathsPage.Values[2],
    'steamvr=' + RuntimePage.Values[0], 'profile=' + ProfileIDs[AccountPage.SelectedValueIndex],
    'mode=' + Modes[ModePage.SelectedValueIndex], 'backups=' + BackupPage.Values[0],
    'layout=' + Layouts[KeyboardPage.SelectedValueIndex],
    'text_locale=' + LocaleCodes[TextPage.SelectedValueIndex],
    'audio_locale=' + LocaleCodes[AudioPage.SelectedValueIndex]];
  if not SaveStringsToUTF8File(ChoicesFile, Lines, False) then RaiseException('Could not save the setup choices.');
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo,
  MemoTypeInfo, MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
var
  Connection: String;
begin
  if ExpandConstant('{param:CONFIG|}') <> '' then begin
    Result := 'Using the supplied setup choices: ' + ExpandConstant('{param:CONFIG|}');
    Exit;
  end;
  Connection := ConnectionPage.CheckListBox.Items[ConnectionPage.SelectedValueIndex];
  Result := 'Game: ' + EditionPage.CheckListBox.Items[EditionPage.SelectedValueIndex] + NewLine +
    'Headset: ' + HeadsetPage.CheckListBox.Items[HeadsetPage.SelectedValueIndex] + NewLine +
    'Connection: ' + Connection + NewLine +
    'Keyboard: ' + KeyboardPage.CheckListBox.Items[KeyboardPage.SelectedValueIndex] + NewLine +
    'Text language: ' + TextPage.CheckListBox.Items[TextPage.SelectedValueIndex] + NewLine +
    'Audio language: ' + AudioPage.CheckListBox.Items[AudioPage.SelectedValueIndex] + NewLine +
    'Mode: ' + ModePage.CheckListBox.Items[ModePage.SelectedValueIndex] + NewLine + NewLine +
    'Game folder: ' + PathsPage.Values[1] + NewLine +
    'Saved Games: ' + PathsPage.Values[2] + NewLine +
    'Steam: ' + PathsPage.Values[0] + NewLine +
    'Steam account: ' + AccountPage.CheckListBox.Items[AccountPage.SelectedValueIndex] + NewLine +
    'Helpers and backups: ' + WizardDirValue + NewLine;
  if BackupPage.Values[0] <> '' then Result := Result + 'Original backup location: ' + BackupPage.Values[0] + NewLine;
  Result := Result + NewLine +
    'Save and exit D2R, then fully exit Steam before installing. Use an offline character in VR.';
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if ExpandConstant('{param:CONFIG|}') <> '' then ChoicesFile := ExpandConstant('{param:CONFIG|}')
  else WriteChoices;
  if not RunBackend(PayloadRoot, '--validate ' + Q(ChoicesFile) + ' --managed-root ' + Q(ExpandConstant('{app}'))) then
    Result := LastOutput + #13#10#13#10 + 'No game or Steam files were changed. Correct the selections, or Save & Exit D2R and fully exit Steam, then retry.';
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then begin
    BackendBusy := True;
    try
      SetupFailed := not RunBackend(ExpandConstant('{app}'), '--install ' + Q(ChoicesFile));
    finally
      BackendBusy := False;
    end;
    if SetupFailed then begin
      Log('Game configuration did not complete. Helper files and backups are retained for repair.');
      MsgBox(LastOutput + #13#10#13#10 + 'Setup did not complete. Your helper files and backups have been kept. See setup-last.log in ' + ExpandConstant('{app}') + ', then rerun Setup to repair.', mbError, MB_OK);
    end;
  end;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpFinished then begin
    if SetupFailed then begin
      WizardForm.FinishedHeadingLabel.Caption := 'D2R PCVR Plus setup needs attention';
      WizardForm.FinishedLabel.Caption := 'Game configuration did not complete. Your backups and helper files were kept. Check setup-last.log in ' + ExpandConstant('{app}') + ' and rerun Setup to repair.';
    end else WizardForm.FinishedLabel.Caption :=
      'Your shortcuts are ready in Steam. Use SteamVR, Virtual Desktop or Flat Screen with an OFFLINE character.'#13#10#13#10 +
      'Normal Steam / Battle.net Play stays stock. First VR load asks for body calibration. Help and controller mappings are in the D2R PCVR Plus Start menu folder.';
  end;
end;

function GetCustomSetupExitCode: Integer;
begin
  if SetupFailed then Result := 1 else Result := 0;
end;

function SetupSucceeded: Boolean;
begin
  Result := not SetupFailed and (ExpandConstant('{param:CONFIG|}') = '');
end;

function SteamExecutable(Param: String): String;
begin
  Result := PathsPage.Values[0] + '\steam.exe';
end;

procedure RegisterPreviousData(PreviousDataKey: Integer);
begin
  if not SetupFailed and (ExpandConstant('{param:CONFIG|}') = '') then begin
    SetPreviousData(PreviousDataKey, 'Headset', IntToStr(HeadsetPage.SelectedValueIndex));
    SetPreviousData(PreviousDataKey, 'Connection', IntToStr(ConnectionPage.SelectedValueIndex));
  end;
end;

procedure CancelButtonClick(CurPageID: Integer; var Cancel, Confirm: Boolean);
begin
  if BackendBusy then begin Cancel := False; Confirm := False; end;
end;

function InitializeUninstall: Boolean;
begin
  Result := not CheckForMutexes('D2R-PCVR-Managed-Operation');
  if Result then CreateMutex('D2R-PCVR-Managed-Operation')
  else begin MsgBox('Another D2R PCVR Plus operation is running. Close it first.', mbError, MB_OK); Exit; end;
  Result := RunBackend(ExpandConstant('{app}'), '--uninstall-check');
  if not Result then MsgBox(LastOutput + #13#10#13#10 + 'Removal stopped. The mod, helper files and backups were kept. Close D2R and fully exit Steam, then retry.', mbError, MB_OK);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then begin
    UninstallProgressForm.StatusLabel.Caption := 'Restoring managed game installations and removing VR shortcuts...';
    if not RunBackend(ExpandConstant('{app}'), '--uninstall') then
      RaiseException(LastOutput + #13#10#13#10 + 'Removal stopped. Helper files and backups were kept.');
  end;
end;

[Messages]
ConfirmUninstall=Remove D2R PCVR Plus from every Steam and Battle.net game copy configured by this installer?%n%nYour characters, shared stash, current video settings and installation backups will be kept.

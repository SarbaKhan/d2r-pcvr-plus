param(
    [string]$ISCCPath,
    [string]$PythonDirectory
)
# Exercise Inno's real lifecycle with a stub backend and a distinct app ID.
# This does not install the mod or touch any real game/Steam configuration.
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
if (!$ISCCPath) { $ISCCPath = Join-Path $repoRoot 'deps/inno-7.1.0/ISCC.exe' }
if (!$PythonDirectory) { $PythonDirectory = Join-Path $repoRoot 'deps/python' }
if (!(Test-Path -LiteralPath $ISCCPath) -or !(Test-Path -LiteralPath (Join-Path $PythonDirectory 'python.exe'))) {
    throw 'Provide Inno Setup 7.1+ and the official embeddable Python directory.'
}
$qaRoot = Join-Path $repoRoot ('build-frame/installer-qa-' + [guid]::NewGuid().ToString('N'))
$payload = Join-Path $qaRoot 'payload'
$game = Join-Path $qaRoot 'Game with spaces'
$target = Join-Path $game 'D2R-PCVR-Plus'
New-Item -ItemType Directory -Path "$payload/steamvr","$payload/.runtime" -Force | Out-Null
Copy-Item -LiteralPath $PythonDirectory -Destination "$payload/.runtime/python" -Recurse
foreach ($name in @('LICENSE','PCVR-PLUS-SETUP.md','CONTROLS.md')) {
    Copy-Item -LiteralPath (Join-Path $repoRoot $name) -Destination (Join-Path $payload $name)
}
$backend = @'
import argparse, configparser
from pathlib import Path
root = Path(__file__).resolve().parent.parent
p = argparse.ArgumentParser()
for option in ('probe','validate','install','managed-root','steam','game','source-root','backup-hint'):
    p.add_argument('--' + option)
for option in ('check','uninstall-check','uninstall'):
    p.add_argument('--' + option, action='store_true')
a = p.parse_args()
if a.probe:
    c = configparser.ConfigParser()
    c['detected'] = dict(profiles='1', profile_0='123', profile_name_0='Fixture only',
                        battlenet_available='1', backup_required='0')
    with Path(a.probe).open('w', encoding='utf-16') as f:
        c.write(f)
elif a.install:
    (root / 'fixture-installed.txt').write_text('stub installed; no game changes')
elif a.uninstall:
    if (root / 'fixture-block-removal.txt').exists():
        raise SystemExit('Fixture: restoration could not finish; helpers must remain.')
    assert (root / '.runtime/python/python.exe').is_file(), 'Helpers removed too early'
    (root.parent / 'removal-ran-before-helper-deletion.txt').write_text('passed')
    (root / 'fixture-installed.txt').unlink()
print('Installer lifecycle fixture passed; no game or Steam configuration touched.')
'@
[IO.File]::WriteAllText((Join-Path $payload 'steamvr/setup_native.py'), $backend)
$script = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'D2R_PCVR_Setup.iss'))
$script = $script.Replace('AppId=D2R-PCVR-Plus-Windows','AppId=D2R-PCVR-Installer-Fixture')
$script = $script.Replace('D2R-PCVR-Managed-Operation','D2R-PCVR-Installer-Fixture-Operation')
$script = $script.Replace('DefaultGroupName=D2R PCVR Plus','DefaultGroupName=D2R PCVR Installer Fixture')
$script = $script.Replace('AppName=D2R PCVR Plus','AppName=D2R PCVR Installer Fixture')
$script = $script.Replace('UninstallDisplayName=D2R PCVR Plus (Windows Flat/VR mod)','UninstallDisplayName=D2R PCVR Installer Fixture')
# The probe deliberately returns no game. Exercise that interactive startup
# state BEFORE /CONFIG supplies the fixture game (silent-only coverage hid it).
$pathChecks = @'
procedure CheckGameDestinationFixture;
var
  Original: String;
begin
  Original := WizardForm.DirEdit.Text;
  if (Length(Original) < 3) or (Copy(Original, 2, 2) <> ':\') then
    RaiseException('Missing game detection left Inno with a relative directory');
  if SelectGameDestination('') or SelectGameDestination('relative\game') or
     SelectGameDestination('C:game') or SelectGameDestination('\game') or
     SelectGameDestination('\\server') or SelectGameDestination('\\server\') then
    RaiseException('Invalid game destination accepted');
  if WizardForm.DirEdit.Text <> Original then
    RaiseException('Invalid input overwrote the last absolute directory');
  if not SelectGameDestination('C:\Games\Diablo II Résurrected') then
    RaiseException('Absolute Unicode game destination rejected');
  if WizardForm.DirEdit.Text <> 'C:\Games\Diablo II Résurrected\D2R-PCVR-Plus' then
    RaiseException('Unicode game destination changed');
  if not SelectGameDestination('\\server\share\Game with spaces') then
    RaiseException('UNC game destination rejected');
  if WizardForm.DirEdit.Text <> '\\server\share\Game with spaces\D2R-PCVR-Plus' then
    RaiseException('UNC game destination changed');
  WizardForm.DirEdit.Text := Original;
  Log('Game destination regression checks passed (missing detection, invalid, Unicode, UNC).');
end;

'@
$script = $script.Replace('procedure InitializeWizard;', $pathChecks + 'procedure InitializeWizard;')
$pathHook = '  SelectGameDestination(PathsPage.Values[1]);'
if (!$script.Contains($pathHook)) { throw 'Game destination regression hook is missing' }
$script = $script.Replace($pathHook, $pathHook + "`r`n  CheckGameDestinationFixture;")
$scriptPath = Join-Path $qaRoot 'Fixture.iss'
[IO.File]::WriteAllText($scriptPath, $script)
& $ISCCPath /Qp "/DPayloadDir=$payload" /DModVersion=0.0.1 "/DInstallerOutput=$qaRoot" $scriptPath
if ($LASTEXITCODE -ne 0) { throw 'Fixture compilation failed' }
$choices = Join-Path $qaRoot 'choices.ini'
[IO.File]::WriteAllText($choices,"[setup]`r`nmode=repair`r`ngame=$game`r`n")
$registryKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\D2R-PCVR-Installer-Fixture_is1'
if (Test-Path -LiteralPath $registryKey) { throw 'A previous fixture remains registered; remove it before testing.' }
$setup = Start-Process -FilePath (Join-Path $qaRoot 'D2R-PCVR-PLUS-Setup-0.0.1.exe') -ArgumentList @(
    '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/DIR="{0}"' -f $target), ('/CONFIG="{0}"' -f $choices), ('/LOG="{0}/setup.log"' -f $qaRoot)
) -WindowStyle Hidden -PassThru -Wait
if ($setup.ExitCode -ne 0 -or !(Test-Path -LiteralPath $registryKey) -or !(Test-Path -LiteralPath "$target/fixture-installed.txt")) {
    throw "Fixture install failed (exit $($setup.ExitCode)). Logs: $qaRoot"
}
$block = Join-Path $target 'fixture-block-removal.txt'
[IO.File]::WriteAllText($block, 'Simulate a restoration failure before Inno deletes helpers.')
$blocked = Start-Process -FilePath "$target/unins000.exe" -ArgumentList @(
    '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/LOG="{0}/blocked-removal.log"' -f $qaRoot)
) -WindowStyle Hidden -PassThru -Wait
if ($blocked.ExitCode -eq 0 -or !(Test-Path -LiteralPath $registryKey) -or
    !(Test-Path -LiteralPath "$target/.runtime/python/python.exe") -or
    !(Test-Path -LiteralPath "$target/fixture-installed.txt")) {
    throw "Blocked removal did not preserve helpers. Logs: $qaRoot"
}
Remove-Item -LiteralPath $block
$removal = Start-Process -FilePath "$target/unins000.exe" -ArgumentList @(
    '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/LOG="{0}/uninstall.log"' -f $qaRoot)
) -WindowStyle Hidden -PassThru -Wait
if ($removal.ExitCode -ne 0 -or (Test-Path -LiteralPath $registryKey) -or
    (Test-Path -LiteralPath "$target/.runtime/python/python.exe") -or
    !(Test-Path -LiteralPath "$game/removal-ran-before-helper-deletion.txt")) {
    throw "Fixture removal failed (exit $($removal.ExitCode)). Logs: $qaRoot"
}
[PSCustomObject]@{Result='Passed'; Scope='Inno file install, Windows Apps registration, failed removal retains helpers, successful removal';
    Backend='Stub only; real game/Steam transactions are covered separately'; Logs=$qaRoot} | ConvertTo-Json

# Build the full installer from this checkout; never read a player installation.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$repo = Split-Path $PSScriptRoot -Parent
$cmake = Join-Path $repo 'deps/cmake-4.4.4-windows-x86_64/bin/cmake.exe'
$ninja = Join-Path $repo 'deps/ninja/ninja.exe'
$python = Join-Path $repo 'deps/python/python.exe'
$xrBuild = Join-Path $repo 'build-frame/openxr-installer'
& $cmake -S "$repo/cmake/openxr-loader" -B $xrBuild -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_TOOLCHAIN_FILE=$repo/cmake/windows-llvm-toolchain.cmake" "-DOPENXR_SDK=$repo/deps/OpenXR-SDK" "-DPython3_EXECUTABLE=$python" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'OpenXR loader configure failed' }
& $cmake --build $xrBuild --target openxr_loader --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'OpenXR loader build failed' }
$loaders = @(Get-ChildItem -LiteralPath $xrBuild -Recurse -Filter openxr_loader.dll)
if ($loaders.Count -ne 1) { throw 'Expected exactly one built OpenXR loader' }
& "$repo/steamvr/build_casclib.ps1"
$compilerSetup = Join-Path $repo 'deps/downloads/innosetup-7.1.0-x64.exe'
if (!(Test-Path -LiteralPath $compilerSetup)) {
    Invoke-WebRequest -UseBasicParsing -Uri 'https://github.com/jrsoftware/issrc/releases/download/is-7_1_0/innosetup-7.1.0-x64.exe' -OutFile $compilerSetup
}
if ((Get-FileHash -LiteralPath $compilerSetup -Algorithm SHA256).Hash.ToLowerInvariant() -ne '0362a383ed217d4c4239b5933866dd96d3eb2102737da92f80f6057a4b40df2f') {
    throw 'Pinned Inno Setup checksum mismatch'
}
$compilerDir = Join-Path $repo 'deps/inno-7.1.0'
$process = Start-Process -FilePath $compilerSetup -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/CURRENTUSER',('/DIR="{0}"' -f $compilerDir)) -Wait -PassThru
if ($process.ExitCode -ne 0) { throw 'Inno Setup tools installation failed' }
$package = Join-Path $repo 'build-frame/releases/pcvr-plus-full.zip'
& $python "$repo/steamvr/package.py" --build "$repo/build-frame/windows-native" --openxr-loader $loaders[0].FullName --casclib "$repo/build-frame/casclib-installer/CascLib.dll" --release-tag v0.1.1-plus --output $package
if ($LASTEXITCODE -ne 0) { throw 'Full package validation failed' }
& "$repo/installer/build_native_installer.ps1" -PackagePath $package -Version '0.1.1' -ISCCPath "$compilerDir/ISCC.exe"
& "$repo/installer/smoke_native_installer.ps1" -ISCCPath "$compilerDir/ISCC.exe"
$payloadLog = Join-Path $repo 'build-frame/installers/payload-check.log'
$process = Start-Process -FilePath "$repo/build-frame/installers/D2R-PCVR-PLUS-Setup-0.1.1.exe" -ArgumentList @('/PAYLOADCHECK=1','/VERYSILENT','/SUPPRESSMSGBOXES',('/LOG="{0}"' -f $payloadLog)) -Wait -PassThru
# PAYLOADCHECK deliberately aborts before installation. Check its success marker.
if (!(Test-Path -LiteralPath $payloadLog) -or !(Select-String -LiteralPath $payloadLog -SimpleMatch 'D2R PCVR Plus PAYLOADCHECK PASSED; no game or Steam files changed.' -Quiet)) {
    throw "Compiled installer payload check failed (exit $($process.ExitCode))"
}

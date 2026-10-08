param([string]$Python='python',[switch]$SkipNative)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    New-Item -ItemType Directory -Force build/release-work | Out-Null
    if(!$SkipNative){
        $env:ZIG_GLOBAL_CACHE_DIR=Join-Path $root 'build/zig-cache'
        $core=@('engine/aw_vm.cpp','engine/aw_music.cpp','engine/aw_pack.cpp','engine/aw_render.cpp','engine/aw_game.cpp','engine/aw_checkpoint.cpp','engine/aw_session.cpp')
        & $Python -m ziglang c++ @core tools/capture_title.cpp -DOR_WIDTH=208 -std=c++11 -O2 -static -w -o build/capture_title.exe > build/release-work/native.log 2>&1
        if($LASTEXITCODE){throw 'Native renderer build failed: build/release-work/native.log'}
    }
    if(!(Test-Path build/device/release/Otherrealm.ino.bin)){throw 'Build device release firmware first.'}
    $bundleArgs=@('-m','PyInstaller','--noconfirm','--onedir','--windowed','--name','Otherrealm-Patcher',
        '--distpath','build/release','--workpath','build/release-work/pyinstaller','--specpath','build/release-work',
        '--paths','tools','--paths','CHGame/tools',
        '--add-binary',((Join-Path $root 'build/capture_title.exe')+':helpers'),
        '--add-data',((Join-Path $root 'assets/title.png')+':assets'),
        '--add-data',((Join-Path $root 'LICENSE')+':.'),'--add-data',((Join-Path $root 'THIRD_PARTY.md')+':.'))
    # Keep the firmware's public name independent of Arduino's sketch naming.
    Copy-Item build/device/release/Otherrealm.ino.bin build/release-work/Otherrealm.bin
    $bundleArgs+=@('--add-data',((Join-Path $root 'build/release-work/Otherrealm.bin')+':firmware'))
    foreach($file in Get-ChildItem firmware/Otherrealm -Filter '*.txt'){
        $bundleArgs+=@('--add-data',($file.FullName+':firmware/Otherrealm'))
    }
    $bundleArgs+=@('tools/patch_game.py')
    & $Python @bundleArgs > build/release-work/pyinstaller.log 2>&1
    if($LASTEXITCODE){throw 'Patcher bundle failed: build/release-work/pyinstaller.log'}
    Copy-Item tools/Upload-Cartridge.ps1 build/release/Otherrealm-Patcher/Upload-Cartridge.ps1
    Copy-Item docs/INSTALL.md build/release/Otherrealm-Patcher/INSTALL.md
    & $Python tools/bundle_notices.py build/release/Otherrealm-Patcher
    if($LASTEXITCODE){throw 'Dependency notice collection failed.'}
    Write-Output 'Built standalone patcher: build/release/Otherrealm-Patcher/Otherrealm-Patcher.exe'
} finally {Pop-Location}

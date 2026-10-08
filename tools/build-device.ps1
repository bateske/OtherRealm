param(
    [switch]$DebugBuild,
    [switch]$PolledSd,
    [string]$ArduinoData = "$env:USERPROFILE\AppData\Local\Arduino15",
    [string]$Sketch = "Otherrealm"
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$env:ARDUINO_DIRECTORIES_DATA = $ArduinoData
$libs = Join-Path $root 'CHGame\platform\board\arduino\CHGame\libraries'
$sketchPath = Join-Path $root "firmware\$Sketch"
$buildKind = if ($DebugBuild) { 'debug' } else { 'release' }
$fqbn = 'CHGame:ch32v:rev0:opt=oslto,rtlib=nano,periph=game'
if (!$DebugBuild) { $fqbn += ',usb=uploadonly' }
$buildFlags = '-DOTHERREALM_DEVICE=1'
$buildFlags += ' "-I' + ((Join-Path $root 'engine') -replace '\\','/') + '"'
if ($DebugBuild) { $buildFlags += ' -DOTHERREALM_DEBUG=1' }
if ($PolledSd) { $buildFlags += ' -DOTHERREALM_FAST_SD=0' }
else { $buildFlags += ' -DOTHERREALM_FAST_SD=1' }
$params = @('compile', '--fqbn', $fqbn, '--build-path', (Join-Path $root "build\device\$buildKind"),
            '--build-property', "build.extra_flags=$buildFlags",
            '--library', (Join-Path $libs 'CHSd'), $sketchPath)
& arduino-cli @params
if ($LASTEXITCODE -ne 0) { throw 'Device compilation failed.' }

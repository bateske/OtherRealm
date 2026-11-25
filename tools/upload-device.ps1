param([string]$Port = 'COM8', [switch]$DebugBuild, [ValidateSet('rev0','rev1')][string]$Revision = 'rev0')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$uploader = Get-ChildItem -LiteralPath "$env:USERPROFILE\AppData\Local\Arduino15\packages\CHGame\tools\chgame-upload" -Recurse -Filter chgame-upload.exe | Select-Object -First 1 -ExpandProperty FullName
if (!$uploader) { throw 'Install the CHGame Arduino board package first.' }
$kind = if ($DebugBuild) { 'debug' } else { 'release' }
$binary = Join-Path $root "build\device\$Revision\$kind\Otherrealm.ino.bin"
if (!(Test-Path -LiteralPath $binary)) { throw "Build firmware first: $binary" }
$busy = Get-CimInstance Win32_Process | Where-Object { $_.Name -eq 'chgame-upload.exe' }
if ($busy) { throw 'Another uploader is running; wait for it to finish.' }
& $uploader -port $Port flash $binary -device $Revision -verify -run
if ($LASTEXITCODE -ne 0) { throw 'Upload failed. Close any serial monitor or Web Serial session holding the port.' }

param(
    [Parameter(Mandatory=$true)][string]$Cartridge,
    [string]$Port,
    [string]$ArduinoData = "$env:LOCALAPPDATA\Arduino15",
    [string]$Python = 'python'
)
# Copy the generated sdcard/ files to the card and safely eject it first.
# This script validates the .chgame, then calls the uploader from Boards Manager.
$ErrorActionPreference='Stop'
$uploader=Get-ChildItem -LiteralPath (Join-Path $ArduinoData 'packages/CHGame/tools/chgame-upload') -Filter chgame-upload.exe -Recurse | Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
if(!$uploader){throw 'Install CHGame through Arduino Boards Manager first, or set -ArduinoData.'}
$cart=(Resolve-Path -LiteralPath $Cartridge).Path
$extract=Join-Path ([IO.Path]::GetTempPath()) ('otherrealm-upload-'+[guid]::NewGuid().ToString('N'))
$app=Join-Path $PSScriptRoot 'Otherrealm-Patcher.exe'
if(Test-Path -LiteralPath $app){
    # PowerShell does not wait for GUI-subsystem executables unless invoked this way.
    $extractArgs='--extract-cart "'+$cart+'" --output "'+$extract+'"'
    $process=Start-Process -FilePath $app -ArgumentList $extractArgs -Wait -PassThru -WindowStyle Hidden
    if($process.ExitCode){throw 'Cartridge validation failed. Use the patcher to regenerate it.'}
}else{
    & $Python (Join-Path $PSScriptRoot 'patch_game.py') --extract-cart $cart --output $extract
    if($LASTEXITCODE){throw 'Cartridge validation failed.'}
}
$binary=Join-Path $extract 'Otherrealm.bin'
if(!(Test-Path -LiteralPath $binary)){throw 'Validated firmware was not produced.'}
$arguments=@('-device','rev0')
if($Port){$arguments+=@('-port',$Port)}
$arguments+=@('flash',$binary,'-verify','-run')
& $uploader @arguments
if($LASTEXITCODE){throw 'Upload failed. Close serial monitors and check the selected port.'}
Write-Output 'Other Realm is running. The SD card must contain OTHERWRL.PAK.'
# Retain the tiny validated extraction as a recoverable upload record in Temp.

param(
    [string]$EmsdkRoot = $env:EMSDK,
    [string]$Pack = 'private/sd/OTHERWRL.PAK'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (!$EmsdkRoot) { $EmsdkRoot = Join-Path (Split-Path $projectRoot -Parent) 'CH32EMU/.tools/emsdk' }
$compiler = Join-Path $EmsdkRoot 'upstream/emscripten/emcc.py'
$pythonCandidates = @(Get-ChildItem -LiteralPath (Join-Path $EmsdkRoot 'python') -Filter python.exe -Recurse)
$nodeCandidates = @(Get-ChildItem -LiteralPath (Join-Path $EmsdkRoot 'node') -Filter node.exe -Recurse)
if (!(Test-Path -LiteralPath $compiler) -or !$pythonCandidates.Count -or !$nodeCandidates.Count) {
    throw 'Supply -EmsdkRoot pointing to an installed Emscripten SDK.'
}
$build = Join-Path $projectRoot 'build/cache-profile'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$savedTemporaryDirectory = $env:EMCC_TEMP_DIR
$savedFrozenCache = $env:EM_FROZEN_CACHE
$sources = @('engine/aw_game.cpp','engine/aw_pack.cpp','engine/aw_render.cpp','engine/aw_music.cpp','engine/aw_vm.cpp')
$variants = @(
    @{Name='unified3';Slots=3;Code=0}, @{Name='split12';Slots=3;Code=1},
    @{Name='split21';Slots=3;Code=2}, @{Name='split22';Slots=4;Code=2},
    @{Name='unified4';Slots=4;Code=0}
)
$rows = @()
$baseline = @{}
try {
    $env:EMCC_TEMP_DIR = $build
    $env:EM_FROZEN_CACHE = '1'
    Push-Location $projectRoot
    try {
        foreach ($variant in $variants) {
            $output = Join-Path $build ($variant.Name + '.js')
            & $pythonCandidates[0].FullName $compiler -x c++ -std=c++11 -O2 -Wall -Wextra -Werror `
                "-DOR_CACHE_SLOTS=$($variant.Slots)" "-DOR_CODE_SLOTS=$($variant.Code)" -Iengine @sources tests/headless.cpp `
                -sENVIRONMENT=node -sNODERAWFS=1 -o $output
            if ($LASTEXITCODE) { throw "Compilation failed for $($variant.Name)." }
            foreach ($part in 16001,16002,16003,16004) {
                $ticks = if ($part -eq 16001) { 3000 } else { 1000 }
                $result = & $nodeCandidates[0].FullName $output $Pack $ticks $part
                if ($LASTEXITCODE) { throw "Replay failed for $($variant.Name), part $part." }
                $line = @($result | Where-Object { $_ -like 'PASS *' })[-1]
                $fields = @{}
                foreach ($match in [regex]::Matches($line, '(\w+)=([^ ]+)')) { $fields[$match.Groups[1].Value] = $match.Groups[2].Value }
                $signature = @($fields.frames,$fields.part,$fields.game_ms,$fields.polygons,$fields.hash) -join '/'
                if ($baseline.ContainsKey($part)) {
                    if ($baseline[$part] -ne $signature) { throw "Replay output mismatch: $($variant.Name), part $part." }
                } else { $baseline[$part] = $signature }
                $row = [pscustomobject]@{ Variant=$variant.Name; StartPart=$part; Ticks=$ticks; Frames=$fields.frames;
                    Sectors=$fields.SD_sectors; MaxSectorsPerTick=$fields.max_sectors_per_tick;
                    GameBytes=$fields.sizeof_game; FinalFrameHash=$fields.hash; Polygons=$fields.polygons }
                $rows += $row
                Write-Output "$($variant.Name) part=$part sectors=$($fields.SD_sectors) max=$($fields.max_sectors_per_tick) hash=$($fields.hash)"
            }
        }
        $rows | Export-Csv -NoTypeInformation -LiteralPath (Join-Path $build 'results.csv')
        Write-Output "All variants produced matching final images, frame counts, simulated timing, and polygon counts. Results: $build/results.csv"
    } finally { Pop-Location }
} finally {
    $env:EMCC_TEMP_DIR = $savedTemporaryDirectory
    $env:EM_FROZEN_CACHE = $savedFrozenCache
}

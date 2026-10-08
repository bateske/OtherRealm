param(
    [string]$EmsdkRoot = $env:EMSDK
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $EmsdkRoot) {
    $EmsdkRoot = Join-Path (Split-Path $projectRoot -Parent) 'CH32EMU/.tools/emsdk'
}
$compiler = Join-Path $EmsdkRoot 'upstream/emscripten/emcc.py'
$pythonCandidates = @(Get-ChildItem -LiteralPath (Join-Path $EmsdkRoot 'python') -Filter python.exe -Recurse)
$nodeCandidates = @(Get-ChildItem -LiteralPath (Join-Path $EmsdkRoot 'node') -Filter node.exe -Recurse)
if (-not (Test-Path -LiteralPath $compiler) -or !$pythonCandidates.Count -or !$nodeCandidates.Count) {
    throw 'Supply -EmsdkRoot pointing to an installed Emscripten SDK.'
}
$testBuild = Join-Path $projectRoot 'build/vm-tests'
New-Item -ItemType Directory -Force -Path $testBuild | Out-Null
$savedTemporaryDirectory = $env:EMCC_TEMP_DIR
$savedFrozenCache = $env:EM_FROZEN_CACHE
try {
    $env:EMCC_TEMP_DIR = $testBuild
    # Reuse the installed SDK without writing cache files outside this project.
    $env:EM_FROZEN_CACHE = '1'
    Push-Location $projectRoot
    try {
        & $pythonCandidates[0].FullName $compiler -x c++ -std=c++11 -O2 -Wall -Wextra -Werror `
            -Iengine engine/aw_vm.cpp engine/aw_music.cpp tests/vm_tests.cpp `
            -sENVIRONMENT=node -o (Join-Path $testBuild 'vm_tests.js')
        if ($LASTEXITCODE) { throw "VM test compilation failed: $LASTEXITCODE" }
        & $nodeCandidates[0].FullName (Join-Path $testBuild 'vm_tests.js')
        if ($LASTEXITCODE) { throw "VM tests failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    $env:EMCC_TEMP_DIR = $savedTemporaryDirectory
    $env:EM_FROZEN_CACHE = $savedFrozenCache
}

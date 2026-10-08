param([string]$EmsdkRoot=$env:EMSDK,[switch]$TestsOnly)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
if(!$EmsdkRoot){$EmsdkRoot=Join-Path (Split-Path $projectRoot -Parent) 'CH32EMU/.tools/emsdk'}
$compiler=Join-Path $EmsdkRoot 'upstream/emscripten/emcc.py'
$python=(Get-ChildItem -LiteralPath (Join-Path $EmsdkRoot 'python') -Filter python.exe -Recurse | Select-Object -First 1).FullName
if(!(Test-Path -LiteralPath $compiler)){throw 'Set -EmsdkRoot to an installed Emscripten SDK.'}
$build=Join-Path $projectRoot 'build/host'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$savedTemp=$env:EMCC_TEMP_DIR;$savedFrozen=$env:EM_FROZEN_CACHE;$savedConfig=$env:EM_CONFIG
try{
 $env:EMCC_TEMP_DIR=$build;$env:EM_FROZEN_CACHE='1';$env:EM_CONFIG=Join-Path $EmsdkRoot '.emscripten'
 Push-Location $projectRoot
 try{
  $core=@('engine/aw_vm.cpp','engine/aw_music.cpp','engine/aw_pack.cpp','engine/aw_render.cpp','engine/aw_game.cpp','engine/aw_checkpoint.cpp','engine/aw_session.cpp')
  & $python $compiler -x c++ @core tests/headless.cpp -std=c++11 -O2 -Wall -Wextra -sENVIRONMENT=node -sNODERAWFS=1 -o build/headless.js
  if($LASTEXITCODE){throw 'Headless build failed'}
  foreach($test in @('vm_tests','demo_tests','pack_tests','render_tests','probe','checkpoint_tests','session_tests')){
   if(Test-Path "tests/$test.cpp"){
    & $python $compiler -x c++ @core "tests/$test.cpp" -Iengine -std=c++11 -O2 -Wall -Wextra -sENVIRONMENT=node -sNODERAWFS=1 -o "build/$test.js"
    if($LASTEXITCODE){throw "$test build failed"}
   }
  }
  & $python $compiler -x c++ firmware/Otherrealm/Persistence.cpp tests/persistence_tests.cpp -DOTHERREALM_PERSISTENCE_TEST -std=c++11 -O2 -Wall -Wextra -sENVIRONMENT=node -sNODERAWFS=1 -o build/persistence_tests.js
  if($LASTEXITCODE){throw 'Persistence test build failed'}
  if(!$TestsOnly){
   & $python $compiler -x c++ @core tools/capture_title.cpp -DOR_WIDTH=208 -std=c++11 -O2 -sENVIRONMENT=node -sNODERAWFS=1 -o build/capture_title.js
   if($LASTEXITCODE){throw 'Title capture build failed'}
   & $python $compiler -x c++ @core web/browser.cpp -std=c++11 -O2 -sENVIRONMENT=web -sMODULARIZE=1 -sEXPORT_NAME=OtherrealmModule -sALLOW_MEMORY_GROWTH=1 '-sEXPORTED_FUNCTIONS=["_malloc","_free"]' '-sEXPORTED_RUNTIME_METHODS=["UTF8ToString","HEAPU8"]' --no-entry -o web/otherrealm.js
   if($LASTEXITCODE){throw 'Browser build failed'}
  }
 }finally{Pop-Location}
}finally{$env:EMCC_TEMP_DIR=$savedTemp;$env:EM_FROZEN_CACHE=$savedFrozen;$env:EM_CONFIG=$savedConfig}

param([Parameter(Mandatory)][string]$DependencyDirectory,[string]$BuildDirectory)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(!$BuildDirectory){$BuildDirectory=Join-Path $repo 'build/phoenix-round4'}
$dep=[IO.Path]::GetFullPath($DependencyDirectory)
$qt=Join-Path $dep 'qt-6.11.3/mingw_64'
$compiler=Join-Path $dep 'qt-6.11.3/Tools/mingw1310_64/bin'
$cmake=Join-Path $dep 'cmake-3.25.0-windows-x86_64/bin/cmake.exe'
. (Join-Path $repo "windows_UI/source-inventory.ps1")
$before=@(Get-SpektraLabSourceInventory $repo)
$beforeDigest=Get-SpektraLabInventoryDigest $before
$env:PATH="$compiler;$qt/bin;$env:PATH"
& $cmake -S $PSScriptRoot -B $BuildDirectory -G 'MinGW Makefiles' '-DCMAKE_BUILD_TYPE=Release' "-DCMAKE_CXX_COMPILER=$compiler/g++.exe" "-DCMAKE_MAKE_PROGRAM=$compiler/mingw32-make.exe" "-DCMAKE_PREFIX_PATH=$qt" '-DBUILD_TESTING=ON' '-DSPEKTRALAB_REQUIRE_VULKAN=ON' "-DSPEKTRALAB_VULKAN_HEADERS=$dep/Vulkan-Headers/include" "-DSPEKTRALAB_VULKAN_LIBRARY=$env:WINDIR/System32/vulkan-1.dll" "-DSPEKTRALAB_GLSLANG=$dep/glslang-build/StandAlone/glslang.exe" "-DSPEKTRALAB_LIBRAW_SOURCE=$dep/LibRaw-0.22.2"
if($LASTEXITCODE){throw 'Configure failed'}
& $cmake --build $BuildDirectory --target spk_phoenix_round4 --parallel 6
if($LASTEXITCODE){throw 'Build failed'}
& "$qt/bin/windeployqt.exe" --release --compiler-runtime --qmldir $PSScriptRoot "$BuildDirectory/app/spk_phoenix_round4.exe"
if($LASTEXITCODE){throw 'Deploy failed'}
Write-Output "Built independent research viewer: $BuildDirectory/app/spk_phoenix_round4.exe"

$after=@(Get-SpektraLabSourceInventory $repo)
if((Get-SpektraLabInventoryDigest $after) -ne $beforeDigest){throw 'Source changed during build'}
$receipt=[ordered]@{format='SpektraLab.PhoenixRound4Build';source_head=(& git -C $repo rev-parse HEAD);source_inventory_sha256=$beforeDigest;source_inventory=$before;executable_sha256=(Get-FileHash -LiteralPath "$BuildDirectory/app/spk_phoenix_round4.exe" -Algorithm SHA256).Hash.ToLowerInvariant();photographic_gate='Not Verified'}
$receipt|ConvertTo-Json -Depth 12|Set-Content -LiteralPath "$BuildDirectory/app/round4-build.json" -Encoding utf8

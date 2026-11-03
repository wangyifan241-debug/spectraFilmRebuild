param(
 [Parameter(Mandatory=$true)][string]$DependencyDirectory,
 [Parameter(Mandatory=$true)][string]$BuildDirectory,
 [int]$Jobs=6
)
$ErrorActionPreference='Stop'
$deps=(Resolve-Path -LiteralPath $DependencyDirectory).Path.Replace('\','/')
$build=[IO.Path]::GetFullPath($BuildDirectory).Replace('\','/')
$compiler="$deps/qt-6.11.3/Tools/mingw1310_64/bin"
$qt="$deps/qt-6.11.3/mingw_64"
$cmake="$deps/cmake-3.25.0-windows-x86_64/bin/cmake.exe"
$env:PATH="$compiler;$qt/bin;"+$env:PATH
& $cmake -S $PSScriptRoot -B $build -G 'MinGW Makefiles' '-DCMAKE_BUILD_TYPE=Release' `
 "-DCMAKE_C_COMPILER=$compiler/gcc.exe" "-DCMAKE_CXX_COMPILER=$compiler/g++.exe" `
 "-DCMAKE_MAKE_PROGRAM=$compiler/mingw32-make.exe" "-DCMAKE_PREFIX_PATH=$qt" `
 "-DSPEKTRALAB_DEPS_DIR=$deps" "-DSPEKTRALAB_LIBRAW_SOURCE=$deps/LibRaw-0.22.2" `
 "-DSPEKTRALAB_VULKAN_HEADERS=$deps/Vulkan-Headers/include" `
 "-DSPEKTRALAB_GLSLANG=$deps/glslang-build/StandAlone/glslangValidator.exe" '-DBUILD_TESTING=ON'
if($LASTEXITCODE -ne 0){throw 'Preview configure failed'}
& $cmake --build $build --target SpektraLabQt spk_qt_phoenix_preview_test spk_qt_qml_workflow_test spk_qt_frame_output_test spk_qt_review_colour_test -j $Jobs
if($LASTEXITCODE -ne 0){throw 'Preview build failed'}

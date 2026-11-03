param(
 [Parameter(Mandatory=$true)][string]$BuildDirectory,
 [Parameter(Mandatory=$true)][string]$QtBin,
 [Parameter(Mandatory=$true)][string]$CompilerBin,
 [Parameter(Mandatory=$true)][string]$LibrarySourceDirectory,
 [Parameter(Mandatory=$true)][string]$Destination
)
$ErrorActionPreference='Stop'
$build=(Resolve-Path -LiteralPath $BuildDirectory).Path
$qt=(Resolve-Path -LiteralPath $QtBin).Path
$compiler=(Resolve-Path -LiteralPath $CompilerBin).Path
$target=[IO.Path]::GetFullPath($Destination)
if(Test-Path -LiteralPath $target){throw 'Choose a new output directory; existing application files will not be replaced'}
$exe=Join-Path $build 'app/SpektraLabPhoenixPreview.exe'
if(!(Test-Path -LiteralPath $exe)){throw 'Build the experimental Qt target first'}
New-Item -ItemType Directory -Path $target | Out-Null
Copy-Item -LiteralPath $exe -Destination $target
foreach($name in @('resources','licenses')){Copy-Item -LiteralPath (Join-Path $build "app/$name") -Destination $target -Recurse}
$qtLicenses=Join-Path $qt '../../licenses'
if(!(Test-Path -LiteralPath $qtLicenses)){throw 'Qt license directory is required beside the Qt installation'}
Copy-Item -LiteralPath $qtLicenses -Destination (Join-Path $target 'licenses/Qt') -Recurse
Copy-Item -LiteralPath (Join-Path $build 'app/spk_material_fit.exe') -Destination $target
$env:PATH=$qt+';'+$compiler+';'+$env:PATH
& (Join-Path $qt 'windeployqt.exe') --release --compiler-runtime --no-translations --qmldir (Join-Path $PSScriptRoot '../qml') (Join-Path $target 'SpektraLabPhoenixPreview.exe')
if($LASTEXITCODE -ne 0){throw "Qt deployment failed: $LASTEXITCODE"}
foreach($name in @('libstdc++-6.dll','libgcc_s_seh-1.dll','libwinpthread-1.dll')){Copy-Item -LiteralPath (Join-Path $compiler $name) -Destination $target -Force}
@'
@echo off
"%~dp0SpektraLabPhoenixPreview.exe" %*
'@ | Set-Content -Encoding ascii (Join-Path $target 'Start-Phoenix.cmd')
@'
@echo off
"%~dp0SpektraLabPhoenixPreview.exe" --fixed-gain %*
'@ | Set-Content -Encoding ascii (Join-Path $target 'Start-Fixed-Gain-Research.cmd')
[IO.File]::WriteAllText((Join-Path $target 'qt.conf'),"[Paths]`nPlugins=.`nQmlImports=qml`n")
$sourceCommit=(& git rev-parse HEAD).Trim()
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
New-Item -ItemType Directory -Path (Join-Path $target 'Source') | Out-Null
$sourceEntries=Get-Content -LiteralPath (Join-Path $root 'windows_UI/dependencies/distribution-sources.json') -Raw | ConvertFrom-Json
foreach($entry in $sourceEntries){
 $file=Join-Path $LibrarySourceDirectory $entry.file
 if((Get-FileHash -LiteralPath $file).Hash.ToLower() -ne $entry.sha256){throw "Library source mismatch: $($entry.file)"}
 Copy-Item -LiteralPath $file -Destination (Join-Path $target 'Source')
}
foreach($name in @('qtbase-sdk-source-overlay.zip','qtbase-sdk-source-overlay.json')){Copy-Item -LiteralPath (Join-Path $LibrarySourceDirectory $name) -Destination (Join-Path $target 'Source')}
Copy-Item -LiteralPath (Join-Path $root 'windows_UI/dependencies/distribution-sources.json') -Destination (Join-Path $target 'Source')
Copy-Item -LiteralPath (Join-Path $root 'windows_UI/THIRD_PARTY.md') -Destination (Join-Path $target 'licenses')
Copy-Item -LiteralPath (Join-Path $qt '../sbom') -Destination (Join-Path $target 'licenses/Qt-SBOM') -Recurse
# Archive only allowlisted source and frozen research inputs, never local photographs/builds.
. (Join-Path $root 'windows_UI/source-inventory.ps1')
$sourceInventory=@(Get-SpektraLabSourceInventory $root)
$researchNames=@('validation/phoenix-round2/profiles','validation/phoenix-round3/profiles',
 'validation/phoenix-round3/calibration-results.json','validation/phoenix-round3/material-plan.v1.json',
 'validation/phoenix-round3/review-hardening/frozen-parent-gains.json','validation/phoenix-round3/research-neutral-print-filters.v1.json')
foreach($name in $researchNames){foreach($file in (Get-ChildItem -LiteralPath (Join-Path $root $name) -File -Recurse)){
 $sourceInventory+=[pscustomobject]@{path=[IO.Path]::GetRelativePath($root,$file.FullName).Replace('\','/');sha256=(Get-FileHash -LiteralPath $file.FullName).Hash.ToLower()}
}}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=[IO.Compression.ZipFile]::Open((Join-Path $target 'Source/SpektraLab-source.zip'),[IO.Compression.ZipArchiveMode]::Create)
try{foreach($entry in $sourceInventory){[void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,(Join-Path $root $entry.path),$entry.path,[IO.Compression.CompressionLevel]::Optimal)}}finally{$zip.Dispose()}
$sourceInventory | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $target 'Source/source-inventory.json')
@"
SpektraLab Phoenix Experimental Preview 0.1
Source commit: $sourceCommit
23 unchanged experimental material profiles; photographic_fidelity_validated=false; Not Verified.
Frozen and neutral-calibrated presets are research starting points, not optimal calibration for arbitrary photographs.
Fixed Internal Gain opens a separate process/store, is Research Only, and does not claim greater physical accuracy.
No Python/Node/Xcode/Qt SDK is required to run this directory. A Vulkan-capable GPU/driver is required.
Source and notices: https://github.com/wangyifan241-debug/spectraFilmRebuild ; bundled licenses and material metadata.
Corresponding application/library sources and pinned Qt SDK overlay are under Source/.
"@ | Set-Content -Encoding utf8 (Join-Path $target 'README.txt')
$entries=@(Get-ChildItem -LiteralPath $target -File -Recurse | ForEach-Object { @{path=[IO.Path]::GetRelativePath($target,$_.FullName).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLower()} })
@{format='SpektraLab.PhoenixPreviewPackage';version=1;source_commit=$sourceCommit;files=$entries} | ConvertTo-Json -Depth 6 | Set-Content -Encoding utf8 (Join-Path $target 'package-sha256.json')
Write-Output $target

param(
 [Parameter(Mandatory=$true)][string]$BuildDirectory,
 [Parameter(Mandatory=$true)][string]$Resources,
 [Parameter(Mandatory=$true)][string]$NormalReference,
 [Parameter(Mandatory=$true)][string]$FixedReference,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$RawInput
)
$ErrorActionPreference='Stop'
$build=(Resolve-Path -LiteralPath $BuildDirectory).Path
$resourcesPath=(Resolve-Path -LiteralPath $Resources).Path
$normal=(Resolve-Path -LiteralPath $NormalReference).Path
$fixed=(Resolve-Path -LiteralPath $FixedReference).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $out){throw 'Use a new evidence directory'}
New-Item -ItemType Directory -Path $out | Out-Null
Copy-Item -LiteralPath $resourcesPath -Destination (Join-Path $out 'resources') -Recurse
$snapshot=Join-Path $out 'resources'
foreach($mode in @('normal','fixed','integrity')){
 $runArgs=@($snapshot,(Join-Path $out $mode),$normal,$fixed)
 if($mode -eq 'fixed'){$runArgs+='--fixed'}
 if($mode -eq 'integrity'){$runArgs+='--integrity-only'}
 if($RawInput -and $mode -ne 'integrity'){$runArgs+=(Resolve-Path -LiteralPath $RawInput).Path}
 & (Join-Path $build 'ui/spk_qt_phoenix_preview_test.exe') @runArgs *> (Join-Path $out "$mode.log")
 if($LASTEXITCODE -ne 0){throw "$mode failed; see retained log"}
}
& (Join-Path $build 'ui/spk_qt_qml_workflow_test.exe') (Join-Path $build 'app/SpektraLabPhoenixPreview.exe') *> (Join-Path $out 'qml.log')
if($LASTEXITCODE -ne 0){throw 'QML workflow failed'}
& (Join-Path $build 'ui/spk_qt_frame_output_test.exe') $snapshot (Join-Path $out 'frame') *> (Join-Path $out 'frame.log')
if($LASTEXITCODE -ne 0){throw 'Frame/export gate failed'}
& (Join-Path $build 'ui/spk_qt_review_colour_test.exe') $snapshot *> (Join-Path $out 'colour.log')
if($LASTEXITCODE -ne 0){throw 'Colour gate failed'}
Write-Output 'PASS all requested preview gates; inspect JSON reports for counts and coverage'

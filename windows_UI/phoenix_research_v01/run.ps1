param([Parameter(Mandatory)][string]$Executable,
      [Parameter(Mandatory)][string]$Raw,
      [Parameter(Mandatory)][string]$NewOutputDirectory)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $NewOutputDirectory){throw 'Output directory must be new'}
$exe=(Resolve-Path -LiteralPath $Executable).Path
$rawPath=(Resolve-Path -LiteralPath $Raw).Path
$resources=Join-Path (Split-Path $exe) 'resources'
New-Item -ItemType Directory -Path $NewOutputDirectory | Out-Null
foreach($inputName in @('synthetic',$rawPath)){
    $label=if($inputName -eq 'synthetic'){'synthetic'}else{'raw'}
    & $exe --render $resources $inputName (Join-Path $NewOutputDirectory $label) *> (Join-Path $NewOutputDirectory "$label.log")
    if($LASTEXITCODE){throw "Render failed: $label"}
}
& python (Join-Path $PSScriptRoot 'analyze.py') $NewOutputDirectory
if($LASTEXITCODE){throw 'Analysis failed'}
$metrics=Get-Content -LiteralPath (Join-Path $NewOutputDirectory 'metrics_and_settings.json') -Raw | ConvertFrom-Json
Write-Output $metrics.status
if(!$metrics.gui_promotion){Write-Output 'Candidate rejected; no preset promotion or further look tuning.'}

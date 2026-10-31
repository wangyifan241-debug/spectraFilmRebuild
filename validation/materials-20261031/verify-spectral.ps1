[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$Responsivity
)
$ErrorActionPreference = 'Stop'
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$tool = Join-Path ([IO.Path]::GetFullPath($BuildDirectory)) 'app/spk_material_fit.exe'
$scan = Join-Path $repository 'engine/resources_research/scan'
$expectedInputs = @{
    'ProImage100_datasheet_curves.json' = 'ec23f1756de5597a86ed779676ba8546595c6e590b9749b53abb41809ee0e75e'
    'ProImage100_dye_density.json' = '342ad845bd70f0f70ea008e8ac670db43e15352260ca12244c14a89baf34c968'
    'ProImage100_spectral_sensitivity.json' = 'b1a6bfe6613b9a6b8430f57fb614edc8c69bb164d35e11c84271029fc94d4ea7'
}
foreach ($name in $expectedInputs.Keys) {
    if ((Get-FileHash -LiteralPath (Join-Path $scan $name) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedInputs[$name]) {
        throw "Pinned photographic input changed: $name"
    }
}
$operatorHash = (Get-FileHash -LiteralPath $Responsivity -Algorithm SHA256).Hash.ToLowerInvariant()
if ($operatorHash -ne 'b296756ebe973d523de62ba2c5d2801f1ecb7f2394d95cf53961746d504c2a32') {
    throw 'This diagnostic regression requires the explicit pinned external operator. It is not distributed here.'
}
# Never reuse/delete existing results. The explicit external table is read only.
$results = Join-Path $repository ('build/spectral-diagnostic-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $results | Out-Null
$spectral = Join-Path $results 'spectral.json'
& $tool --spectral --input (Join-Path $scan 'ProImage100_datasheet_curves.json') `
    --dyes (Join-Path $scan 'ProImage100_dye_density.json') --responsivity $Responsivity `
    --sensitivity (Join-Path $scan 'ProImage100_spectral_sensitivity.json') --output $spectral
$spectralExit = $LASTEXITCODE
if ($spectralExit -ne 3) { throw "Expected real-data quality rejection (exit 3), got $spectralExit" }
$report = Get-Content -LiteralPath $spectral -Raw | ConvertFrom-Json
if ($report.profile_ready -ne $false -or $report.mathematical_fit_passed -ne $false -or
    $report.summary.rows -ne 201 -or $report.summary.rejected_rows -ne 12 -or
    $report.summary.unidentifiable_rows -ne 0) {
    throw 'Real-data diagnostic/refusal contract changed'
}
$spectralHash = (Get-FileHash -LiteralPath $spectral -Algorithm SHA256).Hash.ToLowerInvariant()
if ($spectralHash -ne '59cc7095c750463b75e73e08a6c99edf6aa486e969aeb13c8b2bf326e2a70b93') {
    throw 'Pinned Windows spectral report changed; investigate rather than replacing the golden report'
}
$curve = Join-Path $results 'curve.json'
& $tool --input (Join-Path $scan 'ProImage100_datasheet_curves.json') --output $curve
if ($LASTEXITCODE -ne 0) { throw 'Existing default curve fit failed' }
$curveHash = (Get-FileHash -LiteralPath $curve -Algorithm SHA256).Hash.ToLowerInvariant()
if ($curveHash -ne '79da47609407b5f0c569855d2e067999ba25689d2d3e5dc7057821aa1c3fb0db') {
    throw 'Existing v1 curve report changed'
}
$evidence = [ordered]@{
    diagnostic_regression_passed = $true
    research_quality_gate_passed = $false
    profile_ready = $false
    spectral_cli_exit = $spectralExit
    rejected_rows = $report.summary.rejected_rows
    total_rows = $report.summary.rows
    max_abs_RGB_D = $report.summary.max_abs_RGB_D
    tool_sha256 = (Get-FileHash -LiteralPath $tool -Algorithm SHA256).Hash.ToLowerInvariant()
    operator_sha256 = $operatorHash
    spectral_report_sha256 = $spectralHash
    existing_curve_report_sha256 = $curveHash
    results_directory = $results
}
$evidence | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $results 'verification.json') -Encoding UTF8
$evidence | ConvertTo-Json -Depth 5
Write-Output 'Diagnostic regression passed; real photographic data still REJECTED. No Profile published.'

param([Parameter(Mandatory)][string]$Executable,[Parameter(Mandatory)][string]$SavedEdits,[Parameter(Mandatory)][string]$Output)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Output){throw 'Output exists'}
$exe=(Resolve-Path -LiteralPath $Executable).Path
$bundle=Split-Path $exe -Parent
$build=Get-Content -LiteralPath (Join-Path $bundle 'phoenix-build.json') -Raw | ConvertFrom-Json
$state=Get-Content -LiteralPath $SavedEdits -Raw | ConvertFrom-Json
$manifestPath=Join-Path $bundle 'resources/phoenix-preview.json'
$manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$hashes=[ordered]@{}
Get-ChildItem -LiteralPath (Join-Path $bundle 'resources') -File -Recurse | Sort-Object FullName | ForEach-Object {$relative=[IO.Path]::GetRelativePath($bundle,$_.FullName);$hashes[$relative]=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
$actual=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
if($actual -ne $build.executable_sha256){throw 'Executable differs from build receipt'}
foreach($entry in $manifest.entries){$key='resources\profiles\'+$entry.id+'.json';if($hashes[$key] -ne $entry.sha256){throw "Profile differs: $($entry.id)"}}
$records=[ordered]@{status='RUNTIME_PACKAGE_AND_SAVED_STATE_AUDIT_NOT_LIVE_PARAM_READ';executable=$exe;executable_sha256=$actual;build_receipt=$build;manifest_sha256=(Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant();resource_sha256=$hashes;saved_edits_sha256=(Get-FileHash -LiteralPath $SavedEdits -Algorithm SHA256).Hash.ToLowerInvariant();saved_state=$state;live_resolved_parameters=$null;live_resolved_status='Not Observable: shipped GUI does not expose live engine get_params; saved state is not represented as live state';photographic_gate='Not Verified'}
$records|ConvertTo-Json -Depth 40|Set-Content -LiteralPath $Output -Encoding utf8

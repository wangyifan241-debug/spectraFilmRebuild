[CmdletBinding()]
param([Parameter(Mandatory)][string]$Source,[Parameter(Mandatory)][string]$Destination)
$ErrorActionPreference='Stop'
$profile=Get-Content -Raw -LiteralPath $Source|ConvertFrom-Json -AsHashtable
if(Test-Path -LiteralPath $Destination){throw 'A converted material version must never overwrite an existing file.'}
if($profile.info.stock -ne 'kodak_trix' -or $profile.info.type -ne 'positive' -or $profile.info.channel_model -ne 'bw' -or $profile.data.development_time.Count -ne 1){throw 'Only the audited single-condition 7266 profile is supported.'}
if($profile.data.density_curves_model.model_type -ne 'norm_cdfs'){throw 'Only the existing Gaussian-CDF family is supported.'}
$rawHash=(Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash.ToLowerInvariant()
foreach($key in @('log_sensitivity','channel_density','density_curves')){
    $rows=[Collections.Generic.List[object]]::new()
    foreach($row in $profile.data[$key]){
        if(@($row).Count -ne 1){throw "Unexpected source channel count in $key"}
        $v=$null;if($null -ne $row[0]){$v=[double]$row[0];if($key -eq 'channel_density'){$v/=3}}
        if($null -eq $v -and $key -eq 'density_curves'){throw 'Missing characteristic-curve value'}
        $rows.Add(@($v,$v,$v))
    }
    $profile.data[$key]=@($rows)
}
$layers=[Collections.Generic.List[object]]::new()
foreach($row in $profile.data.density_curves_layers){
    if($row.Count -ne 3){throw 'Expected three grain sublayers'}
    $layer=[Collections.Generic.List[object]]::new();foreach($value in $row){if(@($value).Count -ne 1){throw 'Expected one silver channel'};$layer.Add(@($value[0],$value[0],$value[0]))};$layers.Add(@($layer))
}
$profile.data.density_curves_layers=@($layers)
foreach($key in @('centers','amplitudes','sigmas')){$row=$profile.data.density_curves_model[$key];if($row.Count -ne 1 -or $row[0].Count -ne 3){throw 'Expected a single condition and three CDF layers'};$profile.data.density_curves_model[$key]=@($row[0].Clone(),$row[0].Clone(),$row[0].Clone())}
$profile.data.density_curves_model.model_type='cdfs'
foreach($key in @('midscale_neutral_density','hanatos2025_adaptation_window_params','hanatos2025_adaptation_surface_params')){if($null -eq $profile.data[$key]){$profile.data.Remove($key)}}
$profile.info.stock='open_kodak_trix_7266_033'
$profile.info.name='Kodak Tri-X Reversal 7266 (upstream 0.3.3)'
$profile.metadata.version='0.3.3-spk-r1'
$profile.metadata.source_url='https://github.com/andreavolpato/spektrafilm/blob/6cd00c8d4f30b5b550f50f4bbd3753c9f2a48507/src/spektrafilm/data/profiles/kodak_trix.json'
$profile.metadata.source_sha256=$rawHash
$profile.metadata.quality='Upstream digitized spectral/characteristic data; one supplied development condition. Grain/halation use existing Windows engine defaults, not independently fitted 7266 parameters. No measured full-image validation.'
$profile.metadata.conversion='SpektraLab 2026-10-30, representation only: one silver sensitivity/curve repeated across three channels; dye density divided equally among them to conserve total optical density; sublayers/model repeated; norm_cdfs renamed cdfs (same Gaussian CDF); absent optional null fields omitted. Exposure/wavelength axes and base density unchanged. See CHANGELOG.txt.'
New-Item -ItemType Directory -Path (Split-Path ([IO.Path]::GetFullPath($Destination))) -Force|Out-Null
[IO.File]::WriteAllText([IO.Path]::GetFullPath($Destination),($profile|ConvertTo-Json -Depth 40),[Text.UTF8Encoding]::new($false))
Write-Host "Created new converted ID $($profile.info.stock); original SHA-256 $rawHash"

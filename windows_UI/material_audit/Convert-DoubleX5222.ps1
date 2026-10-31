[CmdletBinding()]
param([Parameter(Mandatory)][string]$Source,[Parameter(Mandatory)][string]$Destination)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Destination){throw 'Never overwrite an existing converted material version.'}
$profile=Get-Content -Raw -LiteralPath $Source|ConvertFrom-Json -AsHashtable
$rawHash=(Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash.ToLowerInvariant()
if($rawHash -ne '37c333046554151ac5538e6eeb1f2cadef919876c63d0dd861c547952874fac3'){throw 'Expected the audited upstream 0.3.3 Double-X source bytes.'}
if($profile.info.stock -ne 'kodak_doublex' -or $profile.info.type -ne 'negative' -or $profile.info.channel_model -ne 'bw' -or ($profile.data.development_time -join ',') -ne '4,5,6.5,9,12'){throw 'Unexpected Double-X condition family.'}
$condition=2 # Explicit source condition: 6.5 minutes. No interpolation or refit.
foreach($key in @('log_sensitivity','channel_density','density_curves')){
    $rows=[Collections.Generic.List[object]]::new()
    foreach($row in $profile.data[$key]){
        $expected=1;if($key -eq 'density_curves'){$expected=5}
        if(@($row).Count -ne $expected){throw "Unexpected source shape: $key"}
        $at=0;if($key -eq 'density_curves'){$at=$condition}
        $v=$null;if($null -ne $row[$at]){$v=[double]$row[$at];if($key -eq 'channel_density'){$v/=3}}
        if($null -eq $v -and $key -eq 'density_curves'){throw 'Missing characteristic curve sample.'}
        $rows.Add(@($v,$v,$v))
    }
    $profile.data[$key]=@($rows)
}
$base=[Collections.Generic.List[object]]::new()
foreach($row in $profile.data.base_density){if($row.Count -ne 5){throw 'Unexpected base-density conditions.'};$base.Add($row[$condition])}
$profile.data.base_density=@($base)
$layers=[Collections.Generic.List[object]]::new()
foreach($row in $profile.data.density_curves_layers){
    if($row.Count -ne 3){throw 'Expected three sublayers.'}
    $layer=[Collections.Generic.List[object]]::new()
    foreach($values in $row){if($values.Count -ne 5){throw 'Unexpected sublayer conditions.'};$v=$values[$condition];$layer.Add(@($v,$v,$v))}
    $layers.Add(@($layer))
}
$profile.data.density_curves_layers=@($layers)
if($profile.data.density_curves_model.model_type -ne 'norm_cdfs'){throw 'Unsupported CDF family.'}
foreach($key in @('centers','amplitudes','sigmas')){$rows=$profile.data.density_curves_model[$key];if($rows.Count -ne 5 -or $rows[$condition].Count -ne 3){throw 'Unexpected CDF conditions.'};$profile.data.density_curves_model[$key]=@($rows[$condition].Clone(),$rows[$condition].Clone(),$rows[$condition].Clone())}
$profile.data.density_curves_model.model_type='cdfs'
$profile.data.development_time=@(6.5)
foreach($key in @('midscale_neutral_density','hanatos2025_adaptation_window_params','hanatos2025_adaptation_surface_params')){if($null -eq $profile.data[$key]){$profile.data.Remove($key)}}
$profile.info.stock='open_kodak_doublex_5222_65m_033'
$profile.info.name='Kodak Double-X 5222 (6.5 min, upstream 0.3.3)'
$profile.metadata.version='0.3.3-spk-r1'
$profile.metadata.source_url='https://github.com/andreavolpato/spektrafilm/blob/6cd00c8d4f30b5b550f50f4bbd3753c9f2a48507/src/spektrafilm/data/profiles/kodak_doublex.json'
$profile.metadata.source_sha256=$rawHash
$profile.metadata.quality='Upstream digitized spectral/characteristic data, fixed supplied 6.5-minute development condition. Other times are not interpolated. Grain/halation use existing engine defaults, not independent Double-X fits. No measured full-image validation.'
$profile.metadata.conversion='SpektraLab 2026-10-31: select condition index 2 (6.5 min) consistently for curve, base density, sublayers and CDF model; repeat silver sensitivity/curve/model/layers across three channels, split dye density by three to conserve total optical density; norm_cdfs renamed cdfs; omit absent optional null fields. Original five conditions archived; no refit or exposure-axis change.'
New-Item -ItemType Directory -Path (Split-Path ([IO.Path]::GetFullPath($Destination))) -Force|Out-Null
[IO.File]::WriteAllText([IO.Path]::GetFullPath($Destination),($profile|ConvertTo-Json -Depth 40),[Text.UTF8Encoding]::new($false))
Write-Host "Created $($profile.info.stock), source SHA256 $rawHash"

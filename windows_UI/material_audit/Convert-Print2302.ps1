[CmdletBinding()]
param([Parameter(Mandatory)][string]$Source,[Parameter(Mandatory)][string]$Destination)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Destination){throw 'Never overwrite an existing converted material version.'}
$rawHash=(Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash.ToLowerInvariant()
if($rawHash -ne '3c04bb2e7701eae97eaaf14100cbf80c8f00dd07db3deda3f14b37ac060ca2be'){throw 'Expected the audited upstream 0.3.3 2302 source bytes.'}
$profile=Get-Content -Raw -LiteralPath $Source|ConvertFrom-Json -AsHashtable
if($profile.info.stock -ne 'kodak_2302' -or $profile.info.support -ne 'film' -or $profile.info.stage -ne 'printing' -or ($profile.data.development_time -join ',') -ne '2,3.5,5,7,9'){throw 'Unexpected print-film condition family.'}
$condition=2 # Fixed supplied 5-minute condition, not a grade or interpolation.
foreach($key in @('log_sensitivity','channel_density','density_curves')){
    $rows=[Collections.Generic.List[object]]::new()
    foreach($row in $profile.data[$key]){
        $expected=1;$at=0;if($key -eq 'density_curves'){$expected=5;$at=$condition}
        if($row.Count -ne $expected){throw "Unexpected source channel/condition shape: $key"};$v=$row[$at]
        if($null -ne $v){$v=[double]$v;if($key -eq 'channel_density'){$v/=3}}
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
    if($row.Count -ne 3){throw 'Expected three sublayers.'};$layer=[Collections.Generic.List[object]]::new()
    foreach($values in $row){if($values.Count -ne 5){throw 'Unexpected sublayer conditions.'};$v=$values[$condition];$layer.Add(@($v,$v,$v))}
    $layers.Add(@($layer))
}
$profile.data.density_curves_layers=@($layers)
if($profile.data.density_curves_model.model_type -ne 'norm_cdfs'){throw 'Unsupported CDF family.'}
foreach($key in @('centers','amplitudes','sigmas')){$rows=$profile.data.density_curves_model[$key];if($rows.Count -ne 5 -or $rows[$condition].Count -ne 3){throw 'Unexpected CDF conditions.'};$profile.data.density_curves_model[$key]=@($rows[$condition].Clone(),$rows[$condition].Clone(),$rows[$condition].Clone())}
$profile.data.density_curves_model.model_type='cdfs';$profile.data.development_time=@(5.0)
foreach($key in @('midscale_neutral_density','hanatos2025_adaptation_window_params','hanatos2025_adaptation_surface_params')){if($null -eq $profile.data[$key]){$profile.data.Remove($key)}}
$profile.info.stock='open_kodak_2302_5m_033'
$profile.info.name='Kodak Print Film 2302 (5 min, upstream 0.3.3)'
$profile.metadata.version='0.3.3-spk-r1'
$profile.metadata.source_url='https://github.com/andreavolpato/spektrafilm/blob/6cd00c8d4f30b5b550f50f4bbd3753c9f2a48507/src/spektrafilm/data/profiles/kodak_2302.json'
$profile.metadata.source_sha256=$rawHash
$profile.metadata.quality='Black-and-white cine print FILM, not photographic paper. Fixed supplied 5-minute condition; unfiltered printing, no Multigrade grade pack. Upstream digitized data; default print/scanner effects, not an independently calibrated optical printer or grain model.'
$profile.metadata.conversion='SpektraLab 2026-10-31: condition index 2 (5 min) selected consistently for base, characteristic curve, sublayers and CDF. Silver sensitivity/curve/model/sublayers repeated across three channels; silver dye density divided by three to conserve total optical density. Keep support=film, stage=printing, axes and original five conditions archived. No refit or interpolation.'
New-Item -ItemType Directory -Path (Split-Path ([IO.Path]::GetFullPath($Destination))) -Force|Out-Null
[IO.File]::WriteAllText([IO.Path]::GetFullPath($Destination),($profile|ConvertTo-Json -Depth 40),[Text.UTF8Encoding]::new($false))
Write-Host "Created $($profile.info.stock); original SHA256 $rawHash"

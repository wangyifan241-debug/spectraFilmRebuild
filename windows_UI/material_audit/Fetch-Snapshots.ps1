[CmdletBinding()]
param([Parameter(Mandatory)][string]$CacheDirectory)
$ErrorActionPreference='Stop'
$cache=[IO.Path]::GetFullPath($CacheDirectory)
$repository=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(-not $cache.StartsWith((Join-Path $repository 'build')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Audit downloads must remain inside a dedicated repository build/ cache, never production resources.'
}
New-Item -ItemType Directory -Path $cache -Force | Out-Null
$snapshots=@(
    @{id='spektrafilm-main';repo='andreavolpato/spektrafilm';branch='main';commit='3bb2c2d2801ff68b92019cf1dbcbb133d60832bc'},
    @{id='spektrafilm-dev';repo='andreavolpato/spektrafilm';branch='dev';commit='6cd00c8d4f30b5b550f50f4bbd3753c9f2a48507'},
    @{id='spektrafilm-experimental';repo='andreavolpato/spektrafilm';branch='experimental';commit='28bf883e1672e884307edc75852549376e13644e'},
    @{id='spektrafilm-couplers';repo='andreavolpato/spektrafilm';branch='couplers-tuning-iterations';commit='a273e49b4243d19d7447df39c2b9880e9ec35a66'},
    @{id='darktable-main';repo='darktable-org/darktable-spektrafilm';branch='main';commit='b15cfe18fb35ec15084f78c986b0ebc730338e6e'},
    @{id='calibration-main';repo='OwlMightyCh/film-scan-calibration';branch='main';commit='8d03a0e0c111625b8a88fb8647a35d95b491249d'}
)
$all=[Collections.Generic.List[object]]::new()
foreach($s in $snapshots) {
    $directory=Join-Path $cache $s.id;New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $treeFile=Join-Path $directory 'github-tree.json'
    if(-not(Test-Path -LiteralPath $treeFile)) {
        Invoke-WebRequest "https://api.github.com/repos/$($s.repo)/git/trees/$($s.commit)?recursive=1" -OutFile $treeFile -TimeoutSec 30
    }
    $tree=Get-Content -Raw -LiteralPath $treeFile|ConvertFrom-Json
    if($tree.sha -ne $s.commit -or $tree.truncated){throw "Incomplete/wrong tree for $($s.id)"}
    $s.tree_sha=$tree.sha
    foreach($f in $tree.tree|Where-Object {$_.type -eq 'blob'}) {
        # No execution of downloaded code. Only material data, licensing,
        # small source/docs and the darktable binary LUT for format research.
        $take=($f.path -match '\.(json|py|md|toml|cff|txt)$|(^|/)(LICENSE(-DATA)?|COPYING)$') -and $f.size -le 500000
        if($f.path -eq 'packs/0.3.3/spectra_lut.f32'){$take=$true}
        if($take){$all.Add([pscustomobject]@{snapshot=$s.id;repo=$s.repo;commit=$s.commit;path=$f.path;blob_sha1=$f.sha;size=$f.size;url="https://raw.githubusercontent.com/$($s.repo)/$($s.commit)/$($f.path)"})}
    }
    Write-Host "$($s.id): complete tree, $(@($tree.tree|Where-Object {$_.path -match '/profiles/[^/]+\.json$'}).Count) profile JSONs"
}
$blobDirectory=Join-Path $cache 'blobs';New-Item -ItemType Directory -Path $blobDirectory -Force | Out-Null
$jobs=@($all|Group-Object blob_sha1|ForEach-Object {$_.Group[0]})
$results=@($jobs|ForEach-Object -Parallel {
    $ErrorActionPreference='Stop';$job=$_;$file=Join-Path $using:blobDirectory $job.blob_sha1
    for($attempt=0;$attempt -lt 3;++$attempt){
        try {
            if(-not(Test-Path -LiteralPath $file)){Invoke-WebRequest $job.url -OutFile $file -TimeoutSec 25}
            $bytes=[IO.File]::ReadAllBytes($file)
            $header=[Text.Encoding]::UTF8.GetBytes("blob $($bytes.Length)`0")
            $object=[byte[]]::new($header.Length+$bytes.Length);[Array]::Copy($header,0,$object,0,$header.Length);[Array]::Copy($bytes,0,$object,$header.Length,$bytes.Length)
            $sha=[Convert]::ToHexString([Security.Cryptography.SHA1]::HashData($object)).ToLowerInvariant()
            if($bytes.Length -ne $job.size -or $sha -ne $job.blob_sha1){throw "Size or Git blob identity mismatch: $($job.path)"}
            [pscustomobject]@{blob_sha1=$sha;sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant();size=$bytes.Length};break
        }catch{if($attempt -eq 2){throw};if(Test-Path -LiteralPath $file){Remove-Item -LiteralPath $file};Start-Sleep -Milliseconds 500}
    }
} -ThrottleLimit 6)
if($results.Count -ne $jobs.Count){throw 'Not every selected blob was verified'}
$digests=@{};foreach($r in $results){$digests[$r.blob_sha1]=$r.sha256}
foreach($f in $all) {
    $destination=Join-Path (Join-Path $cache $f.snapshot) $f.path
    New-Item -ItemType Directory -Path (Split-Path $destination) -Force|Out-Null
    Copy-Item -LiteralPath (Join-Path $blobDirectory $f.blob_sha1) -Destination $destination -Force
    $f|Add-Member -NotePropertyName sha256 -NotePropertyValue $digests[$f.blob_sha1]
}
$manifest=@{format='SpektraLab.MaterialAudit.Snapshots';version=1;retrieved_utc=[DateTime]::UtcNow.ToString('o');snapshots=$snapshots;files=@($all)}
[IO.File]::WriteAllText((Join-Path $cache 'snapshots.json'),($manifest|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
Write-Host "Verified $($all.Count) selected files / $($jobs.Count) unique Git blobs; production resources untouched."

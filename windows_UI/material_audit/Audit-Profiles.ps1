[CmdletBinding()]
param([Parameter(Mandatory)][string]$SnapshotDirectory,[Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$cache=[IO.Path]::GetFullPath($SnapshotDirectory)
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $out){throw 'Use a new output directory; existing audit evidence is never overwritten.'}
New-Item -ItemType Directory -Path $out|Out-Null
$manifest=Get-Content -Raw (Join-Path $cache 'snapshots.json')|ConvertFrom-Json -AsHashtable
Add-Type -TypeDefinition @'
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Security.Cryptography;
using System.Text;
public static class MaterialAuditHash {
    // Versioned typed canonical stream; NOT claimed to be RFC 8785 JSON.
    // Object order/whitespace and 1 versus 1.0 do not affect this hash.
    static void Walk(object value,StringBuilder s) {
        if(value==null){s.Append("Z;");return;}
        if(value is IDictionary d){s.Append('{');var keys=new List<string>();foreach(var k in d.Keys)keys.Add((string)k);keys.Sort(StringComparer.Ordinal);foreach(var k in keys){Walk(k,s);Walk(d[k],s);}s.Append('}');return;}
        if(value is string str){s.Append('S').Append(str.Length).Append(':').Append(str);return;}
        if(value is bool b){s.Append(b?"T;":"F;");return;}
        if(value is IEnumerable a){s.Append('[');foreach(var v in a)Walk(v,s);s.Append(']');return;}
        double n=Convert.ToDouble(value,CultureInfo.InvariantCulture);if(n==0)n=0;
        s.Append('N').Append(n.ToString("R",CultureInfo.InvariantCulture)).Append(';');
    }
    public static string Of(object value){var s=new StringBuilder("spk-audit-v1;");Walk(value,s);return Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(s.ToString()))).ToLowerInvariant();}
}
'@
function JsonFile($path){Get-Content -Raw -LiteralPath $path|ConvertFrom-Json -AsHashtable}
function SaveJson($name,$value){[IO.File]::WriteAllText((Join-Path $out $name),($value|ConvertTo-Json -Depth 70),[Text.UTF8Encoding]::new($false))}
function Shape($value){
    if($null -eq $value){return 'null'}
    if($value -is [Collections.IDictionary]){return 'object{'+(($value.Keys|Sort-Object)-join ',')+'}'}
    if($value -is [array]){if($value.Count -eq 0){return '[0]'};return '['+$value.Count+']'+(Shape $value[0])}
    if($value -is [string]){return 'string'};if($value -is [bool]){return 'bool'};return 'number'
}
function ProfileRecord($dataset,$file,$relative,$url,$blob=''){
    $v=JsonFile $file;$id=[IO.Path]::GetFileNameWithoutExtension($file)
    $fields=[ordered]@{};$shapes=[ordered]@{}
    foreach($key in $v.Keys|Sort-Object){
        $shapes[$key]=Shape $v[$key]
        if($v[$key] -is [Collections.IDictionary]){foreach($child in $v[$key].Keys|Sort-Object){$fields["$key.$child"]=[MaterialAuditHash]::Of($v[$key][$child]);$shapes["$key.$child"]=Shape $v[$key][$child]}}
        else{$fields[$key]=[MaterialAuditHash]::Of($v[$key])}
    }
    $payload=@{};foreach($key in $v.Keys){if($key -ne 'metadata'){$payload[$key]=$v[$key]}}
    [pscustomobject]@{dataset=$dataset;id=$id;declared_id=$v.info.stock;name=$v.info.name;stage=$v.info.stage;type=$v.info.type;channel_model=$v.info.channel_model;version=$v.metadata.version;derived_from=$v.derived_from;
        path=$relative;source_url=$url;git_blob_sha1=$blob;sha256=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant();canonical_sha256=[MaterialAuditHash]::Of($v);material_sha256=[MaterialAuditHash]::Of($payload);data_sha256=[MaterialAuditHash]::Of($v.data);metadata=$v.metadata;fields=$fields;structure=$shapes}
}
$records=[Collections.Generic.List[object]]::new()
foreach($dataset in @(@('local-base','engine/resources/profiles'),@('local-product','engine/resources_product/profiles'),@('windows-bundle','build/qt-20261030-film-pair/app/resources/profiles'))){
    foreach($f in Get-ChildItem -LiteralPath (Join-Path $root $dataset[1]) -Filter '*.json'|Sort-Object Name){$records.Add((ProfileRecord $dataset[0] $f.FullName "$($dataset[1])/$($f.Name)" "local:$($dataset[1])/$($f.Name)"))}
}
foreach($s in $manifest.snapshots|Where-Object {$_.id -ne 'calibration-main'}){
    foreach($f in $manifest.files|Where-Object {$_.snapshot -eq $s.id -and $_.path -match '/profiles/[^/]+\.json$'}){
        $records.Add((ProfileRecord $s.id (Join-Path (Join-Path $cache $s.id) $f.path) $f.path $f.url $f.blob_sha1))
    }
}
$local=@{};foreach($r in $records|Where-Object {$_.dataset -like 'local-*'}){if($local.ContainsKey($r.id)){throw "Duplicate local material ID: $($r.id)"};$local[$r.id]=$r}
# Resolve the one-level alias exactly as load_profile does, for reporting only.
foreach($r in $records|Where-Object {$_.derived_from}){
    $base=$local[$r.derived_from];if(-not $base -or $base.derived_from){throw "Invalid audit alias $($r.id)"}
    $r.stage=$base.stage;$r.type=$base.type;$r.channel_model=$base.channel_model
    $r|Add-Member -NotePropertyName effective_data_sha256 -NotePropertyValue $base.data_sha256
}
$comparisons=[Collections.Generic.List[object]]::new()
foreach($dataset in @('spektrafilm-main','spektrafilm-dev','spektrafilm-experimental','spektrafilm-couplers','darktable-main','windows-bundle')){
    foreach($r in $records|Where-Object {$_.dataset -eq $dataset}){
        $l=$local[$r.id];$changes=@();$status='missing-locally'
        if($l){
            if($l.sha256 -eq $r.sha256){$status='byte-identical'}elseif($l.canonical_sha256 -eq $r.canonical_sha256){$status='format-only'}elseif($l.material_sha256 -eq $r.material_sha256){$status='metadata-only'}else{$status='material-differs'}
            $keys=@(@($l.fields.Keys)+@($r.fields.Keys)|Sort-Object -Unique)
            $changes=@($keys|Where-Object {$l.fields[$_] -ne $r.fields[$_]})
        }
        $comparisons.Add([pscustomobject]@{dataset=$dataset;id=$r.id;name=$r.name;stage=$r.stage;local_version=$l.version;upstream_version=$r.version;status=$status;changed_fields=$changes;local_sha256=$l.sha256;upstream_sha256=$r.sha256;local_data_sha256=$l.data_sha256;upstream_data_sha256=$r.data_sha256})
    }
}
$calibration=@($manifest.files|Where-Object {$_.snapshot -eq 'calibration-main' -and $_.path -match '^data/(films|papers)/.*\.json$'}|ForEach-Object{
    $v=JsonFile (Join-Path (Join-Path $cache $_.snapshot) $_.path)
    [pscustomobject]@{path=$_.path;source_url=$_.url;sha256=$_.sha256;git_blob_sha1=$_.blob_sha1;keys=@($v.Keys);metadata=@($v.GetEnumerator()|Where-Object {$_.Key -match 'source|provenance|license|stock|name|version|note|date|process|illuminant|developer|basis|status|anchor'}|ForEach-Object {@{key=$_.Key;value=$_.Value}});structure=@($v.GetEnumerator()|ForEach-Object {@{field=$_.Key;shape=(Shape $_.Value)}})}
})
SaveJson 'profiles.json' @($records)
SaveJson 'comparisons.json' @($comparisons)
SaveJson 'calibration-files.json' $calibration
SaveJson 'sources.json' @{snapshots=$manifest.snapshots;retrieved_utc=$manifest.retrieved_utc;local_head=(& git -C $root rev-parse HEAD);hash_algorithm='spk-audit-v1 typed canonical stream; SHA-256 raw bytes also retained';verified_download_files=$manifest.files.Count;local_resource_hashes=@($records|Where-Object {$_.dataset -like 'local-*'}|Select-Object path,sha256)}
$lines=[Collections.Generic.List[string]]::new();$lines.Add('# Material inventory / 材料逐项对照');$lines.Add('');$lines.Add('All IDs are filenames. Empty versions are explicitly unversioned; raw and canonical SHA-256 are separate. Full hashes/fields/provenance: profiles.json and comparisons.json.');$lines.Add('')
foreach($dataset in @('local-base','local-product','spektrafilm-main','spektrafilm-dev','spektrafilm-experimental','spektrafilm-couplers','darktable-main')){
    $rows=@($records|Where-Object {$_.dataset -eq $dataset}|Sort-Object stage,id)
    $lines.Add("## $dataset ($($rows.Count))");$lines.Add('');$lines.Add('| ID | Name | Stage/type | Version | SHA-256 (first 12) | Local comparison |');$lines.Add('|---|---|---|---|---|---|')
    foreach($r in $rows){$c=$comparisons|Where-Object {$_.dataset -eq $dataset -and $_.id -eq $r.id};$v=if($r.version){$r.version}else{'unversioned'};$status=if($r.derived_from){'alias: '+$r.derived_from}elseif($c){$c.status}else{'local baseline'};$lines.Add("| $($r.id) | $($r.name) | $($r.stage)/$($r.type) | $v | $($r.sha256.Substring(0,12)) | $status |")}
    $lines.Add('')
}
[IO.File]::WriteAllLines((Join-Path $out 'INVENTORY.md'),$lines,[Text.UTF8Encoding]::new($false))
$comparisons|Group-Object dataset,status|Select-Object Name,Count|Format-Table -AutoSize
Write-Host "Audit generated in $out; no profile/resource writes."

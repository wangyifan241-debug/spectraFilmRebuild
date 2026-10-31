# Qt's release tarball can be newer than its prebuilt SDK. Reconstruct the
# SDK's exact sources using its SPDX inventory and recorded Git revision.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SdkSourceSpdx,
    [Parameter(Mandatory)][string]$ExtractedReleaseSource,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$Revision='5a1194b2d368e2d72c230205c464b04a2f23eccd',
    [switch]$PlanOnly
)
$ErrorActionPreference='Stop'
if($Revision -notmatch '^[0-9a-f]{40}$'){throw 'Expected a fixed Qt Git revision'}
$root=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $root -Force | Out-Null
$source=[IO.Path]::GetFullPath($ExtractedReleaseSource)
$text=Get-Content -LiteralPath $SdkSourceSpdx -Raw
$entries=[regex]::Matches($text,'(?m)^FileName: (.+)\r?\nSPDXID: [^\r\n]+\r?\nFileChecksum: SHA1: ([0-9a-f]{40})')
function Sdk-HashMatches([byte[]]$bytes,[string]$expected){
    if([Convert]::ToHexString([Security.Cryptography.SHA1]::HashData($bytes)).ToLowerInvariant() -eq $expected){return $true}
    # Latin1 is a reversible byte -> char mapping, NOT a source encoding
    # conversion. UTF8 replacement characters corrupt legacy/binary bytes.
    $content=[Text.Encoding]::Latin1.GetString($bytes)
    foreach($s in @($content.Replace("`r`n","`n"),$content.Replace("`r`n","`n").Replace("`n","`r`n"))){
        if([Convert]::ToHexString([Security.Cryptography.SHA1]::HashData([Text.Encoding]::Latin1.GetBytes($s))).ToLowerInvariant() -eq $expected){return $true}
    }
    return $false
}
$needed=[Collections.Generic.List[object]]::new()
$checked=0
foreach($entry in $entries){
    $relative=$entry.Groups[1].Value.Trim().Replace('\','/')
    if($relative.StartsWith('./')){$relative=$relative.Substring(2)}
    if($relative -match '(^|/)\.\.(/|$)|:' -or [IO.Path]::IsPathRooted($relative)){throw 'Unsafe SPDX source path'}
    $file=Join-Path $source $relative
    $expected=$entry.Groups[2].Value
    if((Test-Path -LiteralPath $file -PathType Leaf) -and (Sdk-HashMatches ([IO.File]::ReadAllBytes($file)) $expected)){$checked++;continue}
    $cache=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($relative))).ToLowerInvariant()+'.source'
    $cached=Join-Path $root $cache
    $download=-not((Test-Path -LiteralPath $cached -PathType Leaf) -and (Sdk-HashMatches ([IO.File]::ReadAllBytes($cached)) $expected))
    $needed.Add([pscustomobject]@{path=$relative;expected_sdk_sha1=$expected;cache=$cached;download=$download})
}
Write-Output "SDK source inventory: $checked equivalent files, $($needed.Count) exact-revision replacements required."
ConvertTo-Json -InputObject $needed.ToArray() -Depth 4 | Set-Content -LiteralPath (Join-Path $root 'download-plan.json') -Encoding UTF8
if($PlanOnly){return}
$needed | ForEach-Object -Parallel {
    $ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
    if($_.download){
        $url="https://raw.githubusercontent.com/qt/qtbase/$using:Revision/$($_.path)"
        curl.exe -fLsS --connect-timeout 15 --max-time 60 --retry 2 --retry-delay 1 -o ($_.cache+'.partial') $url
        if($LASTEXITCODE -ne 0){throw "Source download failed: $($_.path)"}
        Move-Item -LiteralPath ($_.cache+'.partial') -Destination $_.cache -Force
    }
} -ThrottleLimit 8
$zipPath=Join-Path $root 'qtbase-sdk-source-overlay.zip'
if(Test-Path -LiteralPath $zipPath){throw 'Overlay output already exists; use a new output directory'}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=[IO.Compression.ZipFile]::Open($zipPath,[IO.Compression.ZipArchiveMode]::Create)
try{
    foreach($entry in $needed){
        if(-not(Sdk-HashMatches ([IO.File]::ReadAllBytes($entry.cache)) $entry.expected_sdk_sha1)){throw "SDK source differs from fixed revision: $($entry.path)"}
        [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$entry.cache,$entry.path,[IO.Compression.CompressionLevel]::Optimal)
    }
}finally{$zip.Dispose()}
[ordered]@{qt='6.11.3';revision=$Revision;source_spdx_sha256=(Get-FileHash -LiteralPath $SdkSourceSpdx -Algorithm SHA256).Hash.ToLowerInvariant()
    checked_files=$entries.Count;equivalent_files=$checked;overlay_files=$needed.Count
    sdk_line_endings='Original bytes, or byte-preserving LF/CRLF normalization only';all_verified=$true
    overlay_sha256=(Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    files=@($needed|Select-Object path,expected_sdk_sha1)} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $root 'qtbase-sdk-source-overlay.json') -Encoding UTF8
Write-Output "Ready: $zipPath"

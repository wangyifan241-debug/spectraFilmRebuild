[CmdletBinding()]
param([Parameter(Mandatory)][string]$Directory,[switch]$VerifyOnly)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($Directory)
$entries=Get-Content -LiteralPath "$PSScriptRoot/dependencies/distribution-sources.json" -Raw|ConvertFrom-Json
foreach($entry in $entries){
    if([IO.Path]::GetFileName($entry.file) -ne $entry.file){throw 'Unsafe manifest filename'}
    $path=Join-Path $root $entry.file
    if(Test-Path -LiteralPath $path){
        if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256){throw "Hash mismatch: $path"}
        continue
    }
    if($VerifyOnly){throw "Missing distribution source: $path"}
    if($entry.generated){throw "Generate $($entry.file) with prepare-qt-sdk-overlay.ps1; see installer/REBUILD.md"}
    New-Item -ItemType Directory -Path $root -Force | Out-Null
    $url=if($entry.mirror){$entry.mirror}else{$entry.url}
    curl.exe -fLsS --connect-timeout 15 --max-time 300 --retry 2 -o ($path+'.partial') $url
    if($LASTEXITCODE -ne 0){throw "Download failed: $url"}
    if((Get-FileHash -LiteralPath ($path+'.partial') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256){throw "Hash mismatch: $url"}
    Move-Item -LiteralPath ($path+'.partial') -Destination $path
}
Write-Output 'Pinned source/license archive hashes verified. The separate Qt SDK source overlay must also be completed before package.ps1 can distribute a build.'

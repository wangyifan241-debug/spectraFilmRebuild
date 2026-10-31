[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PackageDirectory,
    [Parameter(Mandatory)][string]$RawFixture
)
$ErrorActionPreference='Stop'
$package=[IO.Path]::GetFullPath($PackageDirectory)
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(-not $package.StartsWith($repo+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Test only workspace packages'}
$installer=@(Get-ChildItem -LiteralPath $package -Filter '*-setup.exe')
if($installer.Count -ne 1){throw 'Expected one installer'}
$inventory=Get-Content -LiteralPath "$package/payload/Source/source-files.json" -Raw|ConvertFrom-Json
$manifest=Get-Content -LiteralPath "$package/payload/package-manifest.json" -Raw|ConvertFrom-Json
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=[IO.Compression.ZipFile]::OpenRead("$package/payload/Source/SpektraLab-source.zip")
$checked=0
try {
    foreach($file in $inventory){
        $entry=$zip.GetEntry($file.path)
        if(-not $entry){throw "Source archive missing $($file.path)"}
        $stream=$entry.Open();$hasher=[Security.Cryptography.SHA256]::Create()
        try{$actual=[Convert]::ToHexString($hasher.ComputeHash($stream)).ToLowerInvariant()}
        finally{$stream.Dispose();$hasher.Dispose()}
        if($actual -ne $file.sha256){throw "Archived source differs: $($file.path)"}
        $checked++
    }
    if(-not $zip.GetEntry('windows_UI/material_fit/fixtures/proimage100-statusm-fit-v1.json')){throw 'Rebuild regression fixture missing from corresponding source'}
    if(@($zip.Entries|Where-Object {$_.FullName -match '\.(ARW|NEF|DNG|CR2|CR3|RAF|RW2)$'}).Count){throw 'Photographic fixture leaked into source archive'}
}finally{$zip.Dispose()}
$testRoot=Join-Path $package ('render-test-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot|Out-Null
$installed=Join-Path $testRoot 'Standalone application'
$p=Start-Process -FilePath $installer[0].FullName -ArgumentList @('/S','/TESTONLY',"/D=$installed") -WindowStyle Hidden -PassThru -Wait
if($p.ExitCode -ne 0){throw "Isolated install failed: $($p.ExitCode)"}
$raw=Join-Path $testRoot ('owned'+[IO.Path]::GetExtension($RawFixture))
Copy-Item -LiteralPath $RawFixture -Destination $raw
$names=@('PATH','QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QML_IMPORT_PATH','QML2_IMPORT_PATH','QT_QPA_PLATFORM','QSG_RHI_BACKEND','QT_QUICK_BACKEND')
$saved=@{};foreach($name in $names){$saved[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
    $env:PATH="$env:WINDIR/System32;$env:WINDIR"
    foreach($name in $names|Where-Object {$_ -ne 'PATH'}){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:QT_QPA_PLATFORM='windows'
    $png=Join-Path $testRoot 'installed-real-raw.png'
    $arguments=@('--open',('"'+$raw+'"'),'--snapshot',('"'+$png+'"'),'--edit-store',('"'+(Join-Path $testRoot 'private-edits')+'"'))
    $p=Start-Process -FilePath "$installed/SpektraLabQt.exe" -WorkingDirectory $testRoot -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput "$testRoot/stdout.log" -RedirectStandardError "$testRoot/stderr.log"
    if(-not $p.WaitForExit(90000)){$p.Kill();throw 'Installed RAW render timed out'}
    if($p.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $png) -or (Get-Item -LiteralPath $png).Length -lt 10000){throw "Installed native RAW render failed: $($p.ExitCode)"}
    if((Get-Item -LiteralPath "$testRoot/stderr.log").Length){throw 'Installed native render emitted diagnostics; inspect stderr'}
}finally{foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$saved[$name],'Process')}}
[IO.File]::WriteAllText((Join-Path $installed 'recipient-owned.txt'),'Preserve recipient file')
$p=Start-Process -FilePath "$installed/Uninstall.exe" -ArgumentList @('/S',"_?=$installed") -WindowStyle Hidden -PassThru -Wait
if($p.ExitCode -ne 0){throw 'Isolated uninstall failed'}
foreach($file in $manifest.files){if(Test-Path -LiteralPath (Join-Path $installed $file.path)){throw "Uninstall retained a package file: $($file.path)"}}
if(-not(Test-Path -LiteralPath "$installed/recipient-owned.txt")){throw 'Uninstall deleted recipient content'}
[ordered]@{passed=$true;archived_source_files_checked=$checked;regression_fixture_included=$true;raw_photos_not_in_source=$true;installed_native_raw_render=$true;developer_path_removed=$true;native_stderr_empty=$true;uninstalled=$true;recipient_content_preserved=$true;clean_recipient_machine=$false;installer_sha256=(Get-FileHash -LiteralPath $installer[0].FullName -Algorithm SHA256).Hash.ToLowerInvariant();render_sha256=(Get-FileHash -LiteralPath $png -Algorithm SHA256).Hash.ToLowerInvariant();snapshot=$png}|ConvertTo-Json -Depth 4|Set-Content -LiteralPath "$testRoot/report.json" -Encoding UTF8
Get-Content -LiteralPath "$testRoot/report.json"
Write-Output "Evidence: $testRoot/report.json"

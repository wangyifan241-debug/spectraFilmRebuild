[CmdletBinding()]
param([Parameter(Mandatory)][string]$PackageDirectory)
$ErrorActionPreference='Stop'
$package=[IO.Path]::GetFullPath($PackageDirectory)
$repo=[IO.Path]::GetFullPath((Split-Path (Split-Path $PSScriptRoot -Parent) -Parent))
if (-not $package.StartsWith($repo+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Test only workspace packages' }
$installer=@(Get-ChildItem -LiteralPath $package -Filter '*-setup.exe')
if($installer.Count -ne 1){throw 'Expected exactly one installer'}
$manifest=Get-Content -LiteralPath "$package/payload/package-manifest.json" -Raw | ConvertFrom-Json
$testRoot=Join-Path $package ('install-test-'+[Guid]::NewGuid().ToString('N'))
$target=Join-Path $testRoot '安装 测试'
New-Item -ItemType Directory -Path $testRoot | Out-Null
$registry='HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\SpektraLabWindowsPreview'
$registeredBefore=if(Test-Path -LiteralPath $registry){ Get-ItemProperty -LiteralPath $registry | ConvertTo-Json -Compress }else{''}
$process=Start-Process -FilePath $installer[0].FullName -ArgumentList @('/S','/TESTONLY',"/D=$target") -WindowStyle Hidden -PassThru -Wait
if($process.ExitCode -ne 0){throw "Silent install failed: $($process.ExitCode)"}
$checked=0
foreach($file in $manifest.files){
    $installed=Join-Path $target $file.path
    if((Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash.ToLowerInvariant() -ne $file.sha256){throw "Installed file mismatch: $($file.path)"}
    $checked++
}
if((Get-FileHash -LiteralPath "$target/SpektraLabQt.exe" -Algorithm SHA256).Hash.ToLowerInvariant() -ne $manifest.executable_sha256){throw 'Installed EXE hash differs'}
# A second install must fail rather than overwrite even one existing file.
$again=Start-Process -FilePath $installer[0].FullName -ArgumentList @('/S','/TESTONLY',"/D=$target") -WindowStyle Hidden -PassThru
# Message boxes in silent mode must have defaults in the NSIS script.
if(-not $again.WaitForExit(20000)){ $again.Kill();throw 'Overwrite refusal hung in silent mode' }
if($again.ExitCode -ne 2){throw "Expected overwrite refusal 2, got $($again.ExitCode)"}

$names=@('PATH','QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QML_IMPORT_PATH','QML2_IMPORT_PATH','QT_QPA_PLATFORM','QSG_RHI_BACKEND','QT_QUICK_BACKEND')
$saved=@{}
foreach($name in $names){$saved[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try{
    $env:PATH="$env:WINDIR/System32;$env:WINDIR"
    foreach($name in @('QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QML_IMPORT_PATH','QML2_IMPORT_PATH')){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:QT_QPA_PLATFORM='offscreen';$env:QSG_RHI_BACKEND='software';$env:QT_QUICK_BACKEND='software'
    $png=Join-Path $testRoot 'installed-smoke.png'
    $process=Start-Process -FilePath "$target/SpektraLabQt.exe" -WorkingDirectory $testRoot `
        -ArgumentList @('--ui-smoke', ('"'+$png+'"'), '--edit-store', ('"'+(Join-Path $testRoot 'isolated-edits')+'"')) `
        -WindowStyle Hidden -PassThru -RedirectStandardOutput "$testRoot/stdout.log" -RedirectStandardError "$testRoot/stderr.log"
    if(-not $process.WaitForExit(60000)){ $process.Kill();throw 'Installed app smoke timed out' }
    if($process.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $png) -or (Get-Item -LiteralPath $png).Length -lt 1000){throw "Installed app smoke failed: $($process.ExitCode)"}
}finally{foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$saved[$name],'Process')}}

# Leave a recipient-owned file and prove uninstall never removes it.
[IO.File]::WriteAllText((Join-Path $target 'recipient-owned.txt'),'preserve me')
$process=Start-Process -FilePath "$target/Uninstall.exe" -ArgumentList @('/S',"_?=$target") -WindowStyle Hidden -PassThru -Wait
if($process.ExitCode -ne 0){throw "Uninstall failed: $($process.ExitCode)"}
foreach($file in $manifest.files){if(Test-Path -LiteralPath (Join-Path $target $file.path)){throw "Uninstall left package file: $($file.path)"}}
if(-not(Test-Path -LiteralPath "$target/recipient-owned.txt")){throw 'Uninstall removed recipient-owned file'}
if(Test-Path -LiteralPath "$target/spektralab-install.marker"){throw 'Uninstall marker survived'}
$registeredAfter=if(Test-Path -LiteralPath $registry){ Get-ItemProperty -LiteralPath $registry | ConvertTo-Json -Compress }else{''}
if($registeredAfter -ne $registeredBefore){throw 'Test modified real uninstall registration'}
[ordered]@{passed=$true;installed_files_checked=$checked;executable_sha256=$manifest.executable_sha256
    isolated_path=$target;overwrite_refused=$true;installed_app_smoke=$true;uninstalled=$true
    recipient_file_preserved=$true;user_registration_unchanged=$true;clean_machine=$false} |
    ConvertTo-Json | Set-Content -LiteralPath "$testRoot/report.json" -Encoding UTF8
Write-Output "Passed: $checked installed file hashes, isolated smoke, overwrite refusal, safe uninstall. Report: $testRoot/report.json"

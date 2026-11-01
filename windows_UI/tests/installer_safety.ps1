[CmdletBinding()]
param([Parameter(Mandatory)][string]$NsisDirectory,[Parameter(Mandatory)][string]$WorkDirectory)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Split-Path (Split-Path $PSScriptRoot -Parent) -Parent))
$work=[IO.Path]::GetFullPath($WorkDirectory)
if(-not $work.StartsWith($repo+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Use a workspace-owned test directory'}
if(Test-Path -LiteralPath $work){throw 'Use a new owned directory for each run'}
New-Item -ItemType Directory -Path "$work/payload/licenses","$work/payload/data" -Force | Out-Null
[IO.File]::WriteAllText("$work/payload/licenses/LICENSE",'Fixture license')
[IO.File]::WriteAllText("$work/payload/data/managed.txt",'owned package data')
[IO.File]::WriteAllText("$work/payload/SpektraLabQt.exe",'fixture, not an executable')
[IO.File]::WriteAllText("$work/payload/READ-ME-FIRST.txt",'fixture')
@'
Push "$INSTDIR\data"
Call un.ValidatePath
Delete "$INSTDIR\data\managed.txt"
RMDir "$INSTDIR\data"
Delete "$INSTDIR\licenses\LICENSE"
RMDir "$INSTDIR\licenses"
Delete "$INSTDIR\SpektraLabQt.exe"
Delete "$INSTDIR\READ-ME-FIRST.txt"
'@ | Set-Content -LiteralPath "$work/uninstall.nsh" -Encoding utf8
# On the pre-fix script, omit the new guard call to reproduce marker weakness.
$script=Join-Path $repo 'windows_UI/installer/preview.nsi'
if(-not (Select-String -LiteralPath $script -SimpleMatch 'un.ValidatePath' -Quiet)){
    (Get-Content -LiteralPath "$work/uninstall.nsh" | Select-Object -Skip 2) | Set-Content -LiteralPath "$work/uninstall.nsh" -Encoding utf8
}
& "$NsisDirectory/makensis.exe" /V2 /DBUILD_LABEL=review-test "/DPAYLOAD=$(Join-Path $work payload)" "/DOUTPUT_FILE=$(Join-Path $work review-fixture.exe)" "/DUNINSTALL_LIST=$(Join-Path $work uninstall.nsh)" $script
if($LASTEXITCODE -ne 0){throw 'NSIS compilation failed'}
function Run([string]$exe,[string[]]$arguments){
    $p=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(-not $p.WaitForExit(20000)){$p.Kill();throw 'NSIS process deadline exceeded'}
    return $p.ExitCode
}
$physicalRepo=(Get-Item -LiteralPath $repo).ResolveLinkTarget($true)
$runtimeWork=if($physicalRepo){Join-Path $physicalRepo.FullName ([IO.Path]::GetRelativePath($repo,$work))}else{$work}
$target=Join-Path $runtimeWork '安装 测试'
$installCode=Run "$work/review-fixture.exe" @('/S','/TESTONLY',"/D=$target")
if($installCode -ne 0){throw "fresh install failed: $installCode"}
$marker="$target/spektralab-install.marker"
$valid=[IO.File]::ReadAllBytes($marker)
[IO.File]::WriteAllText($marker,'forged-marker')
$code=Run "$target/Uninstall.exe" @('/S',"_?=$target")
if($code -ne 2 -or -not(Test-Path -LiteralPath "$target/data/managed.txt")){throw "Malformed marker was accepted (exit $code)"}
[IO.File]::WriteAllBytes($marker,$valid)
$moved=Join-Path $runtimeWork 'copied-install'
Copy-Item -LiteralPath $target -Destination $moved -Recurse
if((Run "$moved/Uninstall.exe" @('/S',"_?=$moved")) -ne 2){throw 'copied installation binding accepted'}
if(-not(Test-Path -LiteralPath "$moved/data/managed.txt")){throw 'copied install was modified'}
# An attacker-controlled directory junction must not redirect exact-list deletion.
$outside=Join-Path $runtimeWork 'recipient-data';New-Item -ItemType Directory -Path $outside | Out-Null
[IO.File]::WriteAllText("$outside/managed.txt",'recipient photo stand-in')
Move-Item -LiteralPath "$target/data" -Destination "$target/data-retained"
New-Item -ItemType Junction -Path "$target/data" -Target $outside | Out-Null
if((Run "$target/Uninstall.exe" @('/S',"_?=$target")) -ne 2){throw 'junction uninstall accepted'}
if([IO.File]::ReadAllText("$outside/managed.txt") -ne 'recipient photo stand-in'){throw 'junction target data damaged'}
# Remove only the junction itself, never its target.
[IO.Directory]::Delete("$target/data")
Move-Item -LiteralPath "$target/data-retained" -Destination "$target/data"
[IO.File]::WriteAllText("$target/recipient-owned.txt",'preserve')
if((Run "$work/review-fixture.exe" @('/S','/TESTONLY',"/D=$target")) -ne 2){throw 'overwrite accepted'}
if((Run "$target/Uninstall.exe" @('/S',"_?=$target")) -ne 0){throw 'valid uninstall failed'}
if(-not(Test-Path -LiteralPath "$target/recipient-owned.txt")){throw 'recipient file removed'}
if(Test-Path -LiteralPath "$target/data/managed.txt"){throw 'managed data survived valid uninstall'}
$denied=Join-Path $runtimeWork 'write-denied'
New-Item -ItemType Directory -Path $denied | Out-Null
$acl=Get-Acl -LiteralPath $denied
$sid=[Security.Principal.WindowsIdentity]::GetCurrent().User
$rule=[Security.AccessControl.FileSystemAccessRule]::new($sid,[Security.AccessControl.FileSystemRights]::Write,[Security.AccessControl.InheritanceFlags]'ContainerInherit,ObjectInherit',[Security.AccessControl.PropagationFlags]::None,[Security.AccessControl.AccessControlType]::Deny)
$blocked=Get-Acl -LiteralPath $denied;$blocked.AddAccessRule($rule)
try {
    Set-Acl -LiteralPath $denied -AclObject $blocked
    $failure=Run "$work/review-fixture.exe" @('/S','/TESTONLY',"/D=$denied")
    if($failure -eq 0 -or (Test-Path -LiteralPath "$denied/spektralab-install.marker")){throw 'Failed extraction claimed a valid installation'}
}finally{Set-Acl -LiteralPath $denied -AclObject $acl}
$elevated=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
[ordered]@{passed=$true;native_windows=$true;non_admin=(-not $elevated);unicode_path=$true;malformed_marker_refused=$true;copied_binding_refused=$true;junction_refused=$true;overwrite_refused=$true;recipient_file_preserved=$true;write_failure_refused=$true;clean_vm=$false} |
    ConvertTo-Json | Set-Content -LiteralPath "$work/result.json" -Encoding utf8
Write-Output "PASS installer ownership and junction regressions: $work/result.json"

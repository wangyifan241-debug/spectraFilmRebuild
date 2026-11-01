[CmdletBinding()]
param([Parameter(Mandatory)][string]$BuildDirectory)
$ErrorActionPreference='Stop'
. "$PSScriptRoot/../source-inventory.ps1"
$owned=Join-Path ([IO.Path]::GetFullPath($BuildDirectory)) ('payload-identity-'+[Guid]::NewGuid().ToString('N'))
$app=Join-Path $owned 'app';$outside=Join-Path $owned 'recipient'
New-Item -ItemType Directory -Path $app,$outside -Force | Out-Null
[IO.File]::WriteAllText("$app/SpektraLabQt.exe",'same executable')
[IO.File]::WriteAllText("$app/plugin.dll",'tested plugin')
$first=Get-SpektraLabInventoryDigest @(Get-SpektraLabPayloadInventory $app)
[IO.File]::WriteAllText("$app/plugin.dll",'modified plugin')
if((Get-SpektraLabInventoryDigest @(Get-SpektraLabPayloadInventory $app)) -eq $first){throw 'DLL modification was not detected'}
[IO.File]::WriteAllText("$app/plugin.dll",'tested plugin')
if((Get-SpektraLabInventoryDigest @(Get-SpektraLabPayloadInventory $app)) -ne $first){throw 'Restored bytes changed identity'}
[IO.File]::WriteAllText("$app/recipient-photo.txt",'must not accidentally ship')
if((Get-SpektraLabInventoryDigest @(Get-SpektraLabPayloadInventory $app)) -eq $first){throw 'Extra deployment file was not detected'}
New-Item -ItemType Junction -Path "$app/redirected" -Target $outside | Out-Null
$rejected=$false
try{Get-SpektraLabPayloadInventory $app | Out-Null}catch{$rejected=$true}
if(-not $rejected){throw 'Reparse point accepted'}
Write-Output 'PASS deployment identity: stable bytes, DLL mutation, extra file, reparse rejection'

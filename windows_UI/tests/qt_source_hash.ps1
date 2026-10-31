# Regression: LF/CRLF equivalence must never decode/re-encode source bytes.
$ErrorActionPreference='Stop'
$owned=Join-Path ([IO.Path]::GetTempPath()) ('spektralab-source-hash-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path "$owned/release" | Out-Null
$bytes=[byte[]](0xff,0xe9,0x0a,0x61,0x0a)
$windows=[byte[]](0xff,0xe9,0x0d,0x0a,0x61,0x0d,0x0a)
$hash=[Convert]::ToHexString([Security.Cryptography.SHA1]::HashData($windows)).ToLowerInvariant()
[IO.File]::WriteAllBytes("$owned/release/non-utf8.txt",$bytes)
[IO.File]::WriteAllText("$owned/source.spdx","FileName: ./non-utf8.txt`nSPDXID: SPDXRef-Test`nFileChecksum: SHA1: $hash`n")
& "$PSScriptRoot/../prepare-qt-sdk-overlay.ps1" -SdkSourceSpdx "$owned/source.spdx" -ExtractedReleaseSource "$owned/release" -OutputDirectory "$owned/overlay" -PlanOnly
$plan=Get-Content "$owned/overlay/download-plan.json" -Raw
if($plan.Trim() -ne '[]'){throw "Non-UTF8 LF/CRLF-equivalent source was incorrectly rejected. Evidence: $owned"}
# A substantive change must still be rejected, not normalized away.
[IO.File]::WriteAllBytes("$owned/release/non-utf8.txt",[byte[]](0xff,0xe8,0x0a,0x61,0x0a))
& "$PSScriptRoot/../prepare-qt-sdk-overlay.ps1" -SdkSourceSpdx "$owned/source.spdx" -ExtractedReleaseSource "$owned/release" -OutputDirectory "$owned/mutated" -PlanOnly
$rejected=@(Get-Content "$owned/mutated/download-plan.json" -Raw|ConvertFrom-Json)
if($rejected.Count -ne 1 -or -not $rejected[0].download){throw 'Substantive source mutation accepted'}
Write-Output "PASS: byte-preserving non-UTF8 EOL equivalence and mutation refusal. Evidence: $owned"

[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$DependencyDirectory,
    [Parameter(Mandatory)][string]$LibrarySourceDirectory,
    [Parameter(Mandatory)][string]$NsisDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [ValidatePattern('^[0-9A-Za-z.-]+$')][string]$BuildLabel = '2026.10.30-preview.1'
)
$ErrorActionPreference = 'Stop'
$source = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
. "$PSScriptRoot/source-inventory.ps1"
$build = [IO.Path]::GetFullPath($BuildDirectory)
$destination = [IO.Path]::GetFullPath($OutputDirectory)
$app = Join-Path $build 'app'
$makensis = Join-Path ([IO.Path]::GetFullPath($NsisDirectory)) 'makensis.exe'
if (Test-Path -LiteralPath $destination) { throw 'OutputDirectory must be new; existing output is never overwritten.' }
if (-not $destination.StartsWith($source + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Keep generated packaging output inside this source workspace.'
}
$record = Get-Content -LiteralPath "$build/qt-build.json" -Raw | ConvertFrom-Json
if (-not $record.ctest_run) { throw 'Package only a build whose complete Windows CTest gate ran.' }
$sourceInventory = @(Get-SpektraLabSourceInventory $source)
if (-not $record.source_inventory_sha256 -or (Get-SpektraLabInventoryDigest $sourceInventory) -ne $record.source_inventory_sha256) {
    throw 'Working-tree source does not match the tested build. Rebuild first; never ship current source beside an older executable.'
}
$exeHash = (Get-FileHash -LiteralPath "$app/SpektraLabQt.exe" -Algorithm SHA256).Hash.ToLowerInvariant()
if ($exeHash -ne $record.executable_sha256) { throw 'Executable no longer matches qt-build.json.' }
foreach ($file in @($makensis, "$app/platforms/qwindows.dll", "$app/resources/spektrafilm_constants.bin",
        "$app/licenses/LICENSE", "$app/licenses/SPEKTRAFILM_LICENSE.txt", "$app/licenses/Qt/manifest.json")) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Required package input missing: $file" }
}
$librarySources = Get-Content -LiteralPath "$PSScriptRoot/dependencies/distribution-sources.json" -Raw | ConvertFrom-Json
foreach ($entry in $librarySources) {
    $file = Join-Path $LibrarySourceDirectory $entry.file
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {
        throw "Distribution source/license checksum mismatch: $file"
    }
}
# The release Qt base archive differs from the prebuilt SDK. The exact SDK
# source overlay is mandatory, not an optional notice for a later download.
$overlayZip = Join-Path $LibrarySourceDirectory 'qtbase-sdk-source-overlay.zip'
$overlayJson = Join-Path $LibrarySourceDirectory 'qtbase-sdk-source-overlay.json'
if (-not (Test-Path -LiteralPath $overlayZip -PathType Leaf) -or -not (Test-Path -LiteralPath $overlayJson -PathType Leaf)) {
    throw 'Qt SDK corresponding source is incomplete. Finish prepare-qt-sdk-overlay.ps1 before distributing an installer.'
}
$overlay = Get-Content -LiteralPath $overlayJson -Raw | ConvertFrom-Json
$sdkSpdx = Join-Path $DependencyDirectory 'qt-6.11.3/mingw_64/sbom/qtbase-6.11.3.source.spdx'
# Byte-preserving EOL validation recognizes five legacy-encoded release files
# which the old UTF8 conversion incorrectly rejected (22125 + 392 = 22517).
if (-not $overlay.all_verified -or $overlay.checked_files -ne 22517 -or $overlay.overlay_files -ne 392 -or
    $overlay.equivalent_files -ne 22125 -or @($overlay.files).Count -ne 392 -or
    $overlay.sdk_line_endings -ne 'Original bytes, or byte-preserving LF/CRLF normalization only' -or
    $overlay.revision -ne '5a1194b2d368e2d72c230205c464b04a2f23eccd' -or
    $overlay.source_spdx_sha256 -ne (Get-FileHash -LiteralPath $sdkSpdx -Algorithm SHA256).Hash.ToLowerInvariant() -or
    $overlay.overlay_sha256 -ne (Get-FileHash -LiteralPath $overlayZip -Algorithm SHA256).Hash.ToLowerInvariant()) {
    throw 'Qt SDK corresponding-source verification is incomplete or does not match the installed SDK.'
}
New-Item -ItemType Directory -Path $destination | Out-Null
$payload = Join-Path $destination 'payload'
Copy-Item -LiteralPath $app -Destination $payload -Recurse
New-Item -ItemType Directory -Path "$payload/Source", "$payload/licenses/DXC" | Out-Null
Copy-Item -LiteralPath "$PSScriptRoot/installer/READ-ME-FIRST.txt" -Destination "$payload/READ-ME-FIRST.txt"
Copy-Item -LiteralPath "$PSScriptRoot/installer/REBUILD.md" -Destination "$payload/Source/REBUILD.md"
Copy-Item -LiteralPath "$PSScriptRoot/THIRD_PARTY.md" -Destination "$payload/licenses/THIRD_PARTY.md" -Force
Copy-Item -LiteralPath "$PSScriptRoot/dependencies/distribution-sources.json" -Destination "$payload/Source/distribution-sources.json"
Copy-Item -LiteralPath "$NsisDirectory/COPYING" -Destination "$payload/licenses/NSIS-COPYING"
Copy-Item -LiteralPath "$source/engine/third_party/volk/LICENSE.md" -Destination "$payload/licenses/volk-MIT.txt"
Copy-Item -LiteralPath "$source/modern_UI/Spektrafilm/Spektrafilm/Resources/Licenses/Profiles-and-LUTs-CHANGELOG.txt" -Destination "$payload/licenses/Profiles-and-LUTs-CHANGELOG.txt"
foreach ($entry in $librarySources) {
    $folder = if ($entry.file -like 'DXC-*') { "$payload/licenses/DXC" } else { "$payload/Source" }
    Copy-Item -LiteralPath (Join-Path $LibrarySourceDirectory $entry.file) -Destination $folder
}
Copy-Item -LiteralPath $overlayZip,$overlayJson -Destination "$payload/Source"

# Archive the actual Windows source files, including dirty/untracked work,
# rather than 'git archive HEAD' (which would omit the port being distributed).
# Allowlist build inputs/docs; never follow ignored build, SDK or RAW folders.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::Open("$payload/Source/SpektraLab-source.zip", [IO.Compression.ZipArchiveMode]::Create)
$sourceFiles = [Collections.Generic.List[object]]::new()
try {
    foreach ($entry in $sourceInventory) {
        $name=$entry.path
        $path = Join-Path $source $name
        [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $path, $name, [IO.Compression.CompressionLevel]::Optimal)
        $sourceFiles.Add([ordered]@{ path=$name; sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() })
    }
} finally { $zip.Dispose() }
if ((Get-SpektraLabInventoryDigest $sourceFiles) -ne $record.source_inventory_sha256 -or
    (Get-SpektraLabInventoryDigest @(Get-SpektraLabSourceInventory $source)) -ne $record.source_inventory_sha256) {
    throw 'Source changed while packaging; this output is incomplete and must not be distributed.'
}
$sourceFiles | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$payload/Source/source-files.json" -Encoding UTF8
$manifest = [ordered]@{
    product='SpektraLab Windows Preview'; label=$BuildLabel; utc=[DateTime]::UtcNow.ToString('o')
    git_head=((& git -C $source rev-parse HEAD) -join ''); qt=$record.qt; compiler=$record.compiler
    application_build_utc=$record.utc; ctest_run=$record.ctest_run; executable_sha256=$exeHash
    source_inventory_sha256=$record.source_inventory_sha256; qt_sdk_sources_verified=$true
    signed=$false; clean_machine_verified=$false; mac_parity_complete=$false
    files=@(Get-ChildItem -LiteralPath $payload -File -Recurse | Sort-Object FullName | ForEach-Object {
        [ordered]@{path=[IO.Path]::GetRelativePath($payload,$_.FullName).Replace('\','/'); bytes=$_.Length
            sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
    })
}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$payload/package-manifest.json" -Encoding UTF8

# Escape NSIS string metacharacters in an exact uninstall list. Directories
# are removed only if empty; files a recipient added to the app are preserved.
function Escape-Nsis([string]$value) { $value.Replace('$','$$').Replace('"','$\"') }
$uninstall = [Collections.Generic.List[string]]::new()
foreach ($file in Get-ChildItem -LiteralPath $payload -File -Recurse) {
    $relative = Escape-Nsis ([IO.Path]::GetRelativePath($payload, $file.FullName))
    $uninstall.Add('Delete "$INSTDIR\' + $relative + '"')
}
foreach ($dir in (Get-ChildItem -LiteralPath $payload -Directory -Recurse | Sort-Object { $_.FullName.Length } -Descending)) {
    $relative = Escape-Nsis ([IO.Path]::GetRelativePath($payload, $dir.FullName))
    $uninstall.Add('RMDir "$INSTDIR\' + $relative + '"')
}
$uninstallPath = Join-Path $destination 'uninstall-files.nsh'
$uninstall | Set-Content -LiteralPath $uninstallPath -Encoding UTF8
$installer = Join-Path $destination "SpektraLab-$BuildLabel-windows-x64-setup.exe"
& $makensis /V2 "/DBUILD_LABEL=$BuildLabel" "/DPAYLOAD=$payload" "/DOUTPUT_FILE=$installer" "/DUNINSTALL_LIST=$uninstallPath" "$PSScriptRoot/installer/preview.nsi"
if ($LASTEXITCODE -ne 0) { throw "NSIS compiler failed: $LASTEXITCODE" }
$installerHash=(Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
"$installerHash  $([IO.Path]::GetFileName($installer))" | Set-Content -LiteralPath "$destination/SHA256SUMS.txt" -Encoding ASCII
Write-Output "Installer: $installer"
Write-Output "SHA256: $installerHash"

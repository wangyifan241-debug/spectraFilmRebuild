# Shared build/package identity. Include the actual dirty/untracked Windows
# sources and resources, never ignored build outputs or photographs.
function Get-SpektraLabSourceInventory([string]$Root) {
    $names = & git -C $Root -c core.quotepath=false ls-files --cached --others --exclude-standard
    if ($LASTEXITCODE -ne 0) { throw 'Cannot inventory source files' }
    foreach ($name in ($names | Sort-Object -Unique)) {
        if ($name -notmatch '^(engine/|windows_UI/|license/|[^/]+\.(md|txt)$|\.gitignore$)') { continue }
        $path = Join-Path $Root $name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
        if ($name -match '(^|/)(build|\.build|node_modules|\.git)/|\.(ARW|NEF|DNG|CR2|CR3|RAF|RW2)$') { throw "Unexpected source input: $name" }
        [pscustomobject]@{path=$name;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
    }
}
function Get-SpektraLabInventoryDigest($Inventory) {
    $canonical = (($Inventory | ForEach-Object { "$($_.sha256)  $($_.path)" }) -join "`n") + "`n"
    [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($canonical))).ToLowerInvariant()
}

# The tested EXE alone cannot attest to a deployment: DLLs, plugins and baked
# resources are executable inputs too. Refuse links and inventory the exact set.
function Get-SpektraLabPayloadInventory([string]$Root) {
    $rootItem=Get-Item -LiteralPath $Root -Force
    $items=@($rootItem)+@(Get-ChildItem -LiteralPath $Root -Force -Recurse)
    foreach($item in $items){
        if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Package input is a reparse point: $($item.FullName)"}
    }
    foreach($item in ($items | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName)) {
        [pscustomobject]@{path=[IO.Path]::GetRelativePath($Root,$item.FullName).Replace('\','/');sha256=(Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
    }
}

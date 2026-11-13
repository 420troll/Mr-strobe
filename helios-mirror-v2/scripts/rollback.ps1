param([Parameter(Mandatory = $true)][string]$ManifestPath)

$ErrorActionPreference = "Stop"
if (Get-Process -Name Maxwell -ErrorAction SilentlyContinue) {
    throw "Maxwell is running. Close it before rollback."
}
$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
if (-not (Test-Path -LiteralPath $manifest.backupPath -PathType Leaf)) {
    throw "Backup listed in the manifest is missing: $($manifest.backupPath)"
}
$hash = (Get-FileHash -LiteralPath $manifest.backupPath -Algorithm SHA256).Hash
if ($hash -ne $manifest.originalSha256) {
    throw "Backup SHA256 does not match the manifest; refusing to restore."
}
Copy-Item -LiteralPath $manifest.backupPath -Destination $manifest.originalPath -Force
$restored = (Get-FileHash -LiteralPath $manifest.originalPath -Algorithm SHA256).Hash
if ($restored -ne $manifest.originalSha256) {
    throw "Restored file failed SHA256 verification."
}
Write-Host "Original EtherDream.dll restored and verified: $($manifest.originalPath)"

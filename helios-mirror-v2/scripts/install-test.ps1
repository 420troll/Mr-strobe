param(
    [string]$PackageDirectory = (Split-Path -Parent $PSScriptRoot),
    [string]$MaxwellSupportDirectory = "C:\Program Files\Maxwell\resources\support"
)

$ErrorActionPreference = "Stop"
if (Get-Process -Name Maxwell -ErrorAction SilentlyContinue) {
    throw "Maxwell is running. Close it before installing a diagnostic build."
}

$required = @("EtherDream.dll", "HeliosLaserDAC.dll", "libusb-1.0.dll", "mirror.ini")
foreach ($name in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $PackageDirectory $name) -PathType Leaf)) {
        throw "Package is incomplete; missing $name"
    }
}
if (-not (Test-Path -LiteralPath $MaxwellSupportDirectory -PathType Container)) {
    throw "Maxwell support directory not found: $MaxwellSupportDirectory"
}

$original = Join-Path $MaxwellSupportDirectory "EtherDream.dll"
if (-not (Test-Path -LiteralPath $original -PathType Leaf)) {
    throw "Original EtherDream.dll not found; refusing to install."
}

$stamp = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
$backupDirectory = Join-Path $MaxwellSupportDirectory "TROLLfx-backups\$stamp"
New-Item -ItemType Directory -Path $backupDirectory -Force | Out-Null
$backup = Join-Path $backupDirectory "EtherDream.dll.original"
Copy-Item -LiteralPath $original -Destination $backup

$manifest = [ordered]@{
    createdUtc = $stamp
    originalPath = $original
    backupPath = $backup
    originalSha256 = (Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash
    proxySha256 = (Get-FileHash -LiteralPath (Join-Path $PackageDirectory "EtherDream.dll") -Algorithm SHA256).Hash
}
$manifestPath = Join-Path $backupDirectory "rollback-manifest.json"
$manifest | ConvertTo-Json | Set-Content -LiteralPath $manifestPath -Encoding UTF8

foreach ($name in $required + @("TROLLfx-Helios-Mirror-V2.exe")) {
    $source = Join-Path $PackageDirectory $name
    if (Test-Path -LiteralPath $source -PathType Leaf) {
        Copy-Item -LiteralPath $source -Destination (Join-Path $MaxwellSupportDirectory $name) -Force
    }
}

Write-Host "Diagnostic proxy installed only in Maxwell's support directory."
Write-Host "Rollback manifest: $manifestPath"
Write-Warning "This build is unsigned and unvalidated. Start disarmed with both outputs blacked out."

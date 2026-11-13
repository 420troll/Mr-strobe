param(
    [string]$EtherDreamDll = "C:\Program Files\Maxwell\resources\support\EtherDream.dll",
    [string]$MaxwellExternal = "C:\Program Files\Maxwell\resources\externals\jit.maxwell.mxe64"
)

$ErrorActionPreference = "Stop"
foreach ($path in @($EtherDreamDll, $MaxwellExternal)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "File not found: $path"
    }
    $file = Get-Item -LiteralPath $path
    $hash = Get-FileHash -LiteralPath $path -Algorithm SHA256
    [pscustomobject]@{ Path = $file.FullName; Bytes = $file.Length; SHA256 = $hash.Hash }
}

$dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
$llvm = Get-Command llvm-readobj.exe -ErrorAction SilentlyContinue
if ($dumpbin) {
    & $dumpbin.Source /exports $EtherDreamDll
    & $dumpbin.Source /imports $MaxwellExternal
} elseif ($llvm) {
    & $llvm.Source --coff-exports $EtherDreamDll
    & $llvm.Source --coff-imports $MaxwellExternal
} else {
    throw "Install neither tool: run this from a Visual Studio developer shell or provide llvm-readobj.exe."
}

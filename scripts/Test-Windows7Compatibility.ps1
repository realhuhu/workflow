[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0, ValueFromRemainingArguments = $true)]
    [string[]] $Binaries,

    [string] $Dumpbin
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $Dumpbin) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw 'vswhere.exe was not found'
    }
    $vswhereArguments = @(
        '-products', '*',
        '-latest',
        '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
        '-find', 'VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe'
    )
    $Dumpbin = & $vswhere @vswhereArguments |
        Select-Object -First 1
}
if (-not $Dumpbin -or -not (Test-Path -LiteralPath $Dumpbin)) {
    throw "dumpbin.exe was not found: $Dumpbin"
}

$resolvedBinaries = @(
    foreach ($binary in $Binaries) {
        $resolvedPath = (Resolve-Path -LiteralPath $binary).Path
        $item = Get-Item -LiteralPath $resolvedPath
        if ($item.PSIsContainer) {
            Get-ChildItem -LiteralPath $resolvedPath -Recurse -File |
                Where-Object { $_.Extension -in '.dll', '.exe' } |
                ForEach-Object FullName
        } else {
            $resolvedPath
        }
    }
) | Sort-Object -Unique
if (-not $resolvedBinaries) {
    throw 'No PE binaries were found to audit'
}
$providedFileNames = $resolvedBinaries | ForEach-Object {
    [System.IO.Path]::GetFileName($_).ToLowerInvariant()
}
$requiredApiSetShims = @(
    'api-ms-win-core-libraryloader-l1-2-0.dll',
    'api-ms-win-core-processtopology-obsolete-l1-1-0.dll',
    'api-ms-win-eventing-provider-l1-1-0.dll'
)
$expectedShimExports = @{
    'api-ms-win-core-libraryloader-l1-2-0.dll' = @(
        'FreeLibrary',
        'GetModuleHandleExW',
        'GetModuleHandleW',
        'GetModuleHandleA',
        'GetModuleFileNameA',
        'GetProcAddress',
        'LoadLibraryExW'
    )
    'api-ms-win-core-processtopology-obsolete-l1-1-0.dll' = @(
        'SetThreadAffinityMask'
    )
    'api-ms-win-eventing-provider-l1-1-0.dll' = @(
        'EventWriteTransfer',
        'EventUnregister',
        'EventRegister'
    )
}

$forbiddenImports = @(
    'CreateFile2',
    'CopyFile2',
    'GetSystemTimePreciseAsFileTime',
    'WaitOnAddress',
    'WakeByAddressAll',
    'WakeByAddressSingle',
    'SetThreadDescription',
    'GetThreadDescription',
    'GetDpiForWindow',
    'GetDpiForSystem',
    'AdjustWindowRectExForDpi',
    'SetProcessDpiAwarenessContext',
    'api-ms-win-core-path-l1-1-0.dll',
    'api-ms-win-core-winrt'
)
$forbiddenPattern = ($forbiddenImports | ForEach-Object { [regex]::Escape($_) }) -join '|'

foreach ($resolvedBinary in $resolvedBinaries) {
    $headers = & $Dumpbin /headers $resolvedBinary 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin /headers failed for $resolvedBinary"
    }
    $subsystemMatch = [regex]::Match($headers, '(?m)^\s*(\d+)\.(\d+)\s+subsystem version\s*$')
    if (-not $subsystemMatch.Success) {
        throw "PE subsystem version was not found in $resolvedBinary"
    }
    $major = [int] $subsystemMatch.Groups[1].Value
    $minor = [int] $subsystemMatch.Groups[2].Value
    if ($major -gt 6 -or ($major -eq 6 -and $minor -gt 1)) {
        throw "$resolvedBinary requires subsystem $major.$minor, later than Windows 7"
    }

    $imports = & $Dumpbin /imports $resolvedBinary 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin /imports failed for $resolvedBinary"
    }
    $blocked = [regex]::Matches($imports, $forbiddenPattern, 'IgnoreCase') |
        ForEach-Object Value |
        Sort-Object -Unique
    if ($blocked) {
        throw "$resolvedBinary imports post-Windows-7 APIs: $($blocked -join ', ')"
    }
    foreach ($shim in $requiredApiSetShims) {
        if ($imports -match [regex]::Escape($shim) -and $shim -notin $providedFileNames) {
            throw "$resolvedBinary requires the missing Windows 7 API-set shim $shim"
        }
    }
    $fileName = [System.IO.Path]::GetFileName($resolvedBinary).ToLowerInvariant()
    if ($expectedShimExports.ContainsKey($fileName)) {
        $exports = & $Dumpbin /exports $resolvedBinary 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) {
            throw "dumpbin /exports failed for $resolvedBinary"
        }
        foreach ($expectedExport in $expectedShimExports[$fileName]) {
            if ($exports -notmatch "(?m)\b$([regex]::Escape($expectedExport))\b") {
                throw "$resolvedBinary does not export $expectedExport"
            }
        }
    }
    Write-Host "Windows 7 import audit passed: $resolvedBinary (subsystem $major.$minor)"
}

Write-Host 'Static PE audit passed. A real Windows 7 SP1 smoke test remains required before release.'

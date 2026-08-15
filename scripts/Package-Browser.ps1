[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $SourceDirectory,

    [Parameter(Mandatory = $true)]
    [string] $DestinationDirectory,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string] $Version,

    [string] $VcRuntimeDirectory,

    [string] $UcrtRuntimeDirectory,

    [string] $Dumpbin
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$crtNames = @(
    'concrt140.dll',
    'msvcp140.dll',
    'msvcp140_1.dll',
    'msvcp140_2.dll',
    'msvcp140_atomic_wait.dll',
    'msvcp140_codecvt_ids.dll',
    'vcruntime140.dll',
    'vcruntime140_1.dll'
)
$win7ShimNames = @(
    'api-ms-win-core-libraryloader-l1-2-0.dll',
    'api-ms-win-core-processtopology-obsolete-l1-1-0.dll',
    'api-ms-win-eventing-provider-l1-1-0.dll',
    'api-ms-win-core-heap-l2-1-0.dll',
    'api-ms-win-core-shlwapi-legacy-l1-1-0.dll'
)
$commonRequiredPaths = @(
    'workflow-browser-harness.exe',
    'README.md',
    'browser-test\index.html',
    'models\det.onnx',
    'models\rec.onnx',
    'onnxruntime.dll',
    'Qt5Core.dll',
    'Qt5WebEngineCore.dll',
    'Qt5WebEngineWidgets.dll',
    'QtWebEngineProcess.exe',
    'resources\icudtl.dat',
    'licenses\Qt-LGPL-GPL-3.0.txt'
)

function Resolve-ExistingDirectory(
    [string] $Path,
    [string] $Description
) {
    if (-not $Path) {
        return $null
    }
    $resolved = [System.IO.Path]::GetFullPath($Path)
    if (-not (Test-Path -LiteralPath $resolved -PathType Container)) {
        throw "$Description was not found: $resolved"
    }
    return $resolved
}

function Find-VcRuntimeDirectory {
    if ($VcRuntimeDirectory) {
        return Resolve-ExistingDirectory $VcRuntimeDirectory 'MSVC v142 runtime directory'
    }

    $candidates = [System.Collections.Generic.List[string]]::new()
    if ($env:VCToolsRedistDir) {
        $candidates.Add((Join-Path $env:VCToolsRedistDir 'x64\Microsoft.VC142.CRT'))
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $installations = @(
            & $vswhere -products '*' `
                -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                -property installationPath
        )
        foreach ($installation in $installations) {
            $redistRoot = Join-Path $installation 'VC\Redist\MSVC'
            if (-not (Test-Path -LiteralPath $redistRoot -PathType Container)) {
                continue
            }
            Get-ChildItem -LiteralPath $redistRoot -Directory |
                Where-Object Name -Like '14.29.*' |
                Sort-Object { [version] $_.Name } -Descending |
                ForEach-Object {
                    $candidates.Add((Join-Path $_.FullName 'x64\Microsoft.VC142.CRT'))
                }
        }
    }

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath (Join-Path $candidate 'msvcp140.dll') -PathType Leaf) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }
    throw 'The Visual Studio 2019 v142 x64 redistributable directory was not found'
}

function Find-UcrtRuntimeDirectory {
    if ($UcrtRuntimeDirectory) {
        return Resolve-ExistingDirectory $UcrtRuntimeDirectory 'UCRT runtime directory'
    }

    $redistRoot = "${env:ProgramFiles(x86)}\Windows Kits\10\Redist"
    if (-not (Test-Path -LiteralPath $redistRoot -PathType Container)) {
        throw "Windows SDK redist directory was not found: $redistRoot"
    }

    $preferred = Join-Path $redistRoot '10.0.19041.0\ucrt\DLLs\x64'
    $candidates = [System.Collections.Generic.List[string]]::new()
    $candidates.Add($preferred)
    Get-ChildItem -LiteralPath $redistRoot -Directory |
        Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
        Sort-Object { [version] $_.Name } -Descending |
        ForEach-Object {
            $candidates.Add((Join-Path $_.FullName 'ucrt\DLLs\x64'))
        }

    foreach ($candidate in $candidates | Select-Object -Unique) {
        if (Test-Path -LiteralPath (Join-Path $candidate 'ucrtbase.dll') -PathType Leaf) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }
    throw 'The x64 app-local UCRT directory was not found'
}

function Find-Dumpbin {
    if ($Dumpbin) {
        $resolved = [System.IO.Path]::GetFullPath($Dumpbin)
        if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
            throw "dumpbin.exe was not found: $resolved"
        }
        return $resolved
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        throw 'vswhere.exe was not found'
    }
    $resolved = & $vswhere -products '*' -latest `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -find 'VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe' |
        Select-Object -First 1
    if (-not $resolved -or -not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
        throw 'dumpbin.exe was not found'
    }
    return $resolved
}

function Assert-ChildPath(
    [string] $Path,
    [string] $Parent
) {
    $resolvedPath = [System.IO.Path]::GetFullPath($Path)
    $resolvedParent = [System.IO.Path]::GetFullPath($Parent).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar
    ) + [System.IO.Path]::DirectorySeparatorChar
    if (-not $resolvedPath.StartsWith(
        $resolvedParent,
        [System.StringComparison]::OrdinalIgnoreCase
    )) {
        throw "Refusing to modify a path outside $resolvedParent`: $resolvedPath"
    }
    return $resolvedPath
}

function Reset-ChildDirectory(
    [string] $Path,
    [string] $Parent
) {
    $resolved = Assert-ChildPath $Path $Parent
    if (Test-Path -LiteralPath $resolved) {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
    New-Item -ItemType Directory -Path $resolved | Out-Null
    return $resolved
}

function Assert-CommonPackageFiles(
    [string] $Root
) {
    foreach ($relativePath in $commonRequiredPaths) {
        $path = Join-Path $Root $relativePath
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Browser package is missing $relativePath"
        }
    }
    $opencvRuntime = Get-ChildItem -LiteralPath $Root -File |
        Where-Object Name -Match '^opencv_(world|core)4100\.dll$' |
        Select-Object -First 1
    if (-not $opencvRuntime) {
        throw 'Browser package is missing the OpenCV 4.10 runtime'
    }
}

function Assert-CompatibleApiSetClosure(
    [string] $Root,
    [string] $DumpbinPath
) {
    $provided = @(
        Get-ChildItem -LiteralPath $Root -Recurse -File |
            ForEach-Object { $_.Name.ToLowerInvariant() }
    )
    $imports = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase
    )
    Get-ChildItem -LiteralPath $Root -Recurse -File |
        Where-Object Extension -In '.dll', '.exe' |
        ForEach-Object {
            $output = & $DumpbinPath /nologo /imports $_.FullName 2>$null | Out-String
            foreach ($match in [regex]::Matches(
                $output,
                '(?im)^\s+((?:api-ms-win|ext-ms-win)[a-z0-9_.-]*\.dll)\s*$'
            )) {
                [void] $imports.Add($match.Groups[1].Value)
            }
        }
    $missing = @(
        $imports |
            Where-Object { $_.ToLowerInvariant() -NotIn $provided } |
            Sort-Object
    )
    if ($missing) {
        throw "Compatible package has unresolved API-set imports: $($missing -join ', ')"
    }
}

function Write-Checksum(
    [string] $Archive
) {
    $checksum = "$Archive.sha256"
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $Archive).Hash.ToLowerInvariant()
    $line = "$hash  $([System.IO.Path]::GetFileName($Archive))"
    [System.IO.File]::WriteAllText($checksum, $line, [System.Text.Encoding]::ASCII)
    return $checksum
}

$source = Resolve-ExistingDirectory $SourceDirectory 'Browser source directory'
$destination = [System.IO.Path]::GetFullPath($DestinationDirectory)
New-Item -ItemType Directory -Force -Path $destination | Out-Null
$vcRuntime = Find-VcRuntimeDirectory
$ucrtRuntime = Find-UcrtRuntimeDirectory
$dumpbinPath = Find-Dumpbin

foreach ($name in $crtNames) {
    if (-not (Test-Path -LiteralPath (Join-Path $vcRuntime $name) -PathType Leaf)) {
        throw "MSVC v142 runtime is missing $name"
    }
}
foreach ($name in $win7ShimNames) {
    if (-not (Test-Path -LiteralPath (Join-Path $source $name) -PathType Leaf)) {
        throw "Browser source directory is missing the Win7 shim $name"
    }
}

$workRoot = Reset-ChildDirectory `
    (Join-Path $destination ".browser-package-$([guid]::NewGuid().ToString('N'))") `
    $destination
$slimRoot = New-Item -ItemType Directory -Path (Join-Path $workRoot 'slim')
$compatibleRoot = New-Item -ItemType Directory -Path (Join-Path $workRoot 'compatible')
Get-ChildItem -LiteralPath $source -Force |
    Copy-Item -Destination $slimRoot.FullName -Recurse -Force
Get-ChildItem -LiteralPath $source -Force |
    Copy-Item -Destination $compatibleRoot.FullName -Recurse -Force
$browserReadme = [System.IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot '..\tests\browser\README.md')
)
Copy-Item -LiteralPath $browserReadme `
    -Destination (Join-Path $slimRoot.FullName 'README.md') -Force
Copy-Item -LiteralPath $browserReadme `
    -Destination (Join-Path $compatibleRoot.FullName 'README.md') -Force

$slimRuntimePattern = '^(concrt140|msvcp140.*|vcruntime140.*|vccorlib140|ucrtbase|api-ms-win-(?:crt|core|eventing).*)\.dll$'
Get-ChildItem -LiteralPath $slimRoot.FullName -File |
    Where-Object Name -Match $slimRuntimePattern |
    ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }

foreach ($name in $crtNames) {
    Copy-Item -LiteralPath (Join-Path $vcRuntime $name) `
        -Destination $compatibleRoot.FullName -Force
}
Get-ChildItem -LiteralPath $ucrtRuntime -File -Filter '*.dll' |
    Copy-Item -Destination $compatibleRoot.FullName -Force

$slimDescription = @(
    'Variant: slim',
    'Requires: Windows 10 x64 and Microsoft Visual C++ 2015-2022 Redistributable x64.',
    'Contains: Workflow Browser UI, Qt, OpenCV, ONNX Runtime, OCR models, fixtures, and licenses.'
)
$compatibleDescription = @(
    'Variant: Windows 7 SP1 compatible',
    'Requires: Windows 7 SP1 x64 or newer.',
    'Contains: slim files plus MSVC v142 CRT, app-local UCRT, and Win7 API-set forwarders.'
)
[System.IO.File]::WriteAllLines(
    (Join-Path $slimRoot.FullName 'PACKAGE_VARIANT.txt'),
    $slimDescription,
    [System.Text.UTF8Encoding]::new($false)
)
[System.IO.File]::WriteAllLines(
    (Join-Path $compatibleRoot.FullName 'PACKAGE_VARIANT.txt'),
    $compatibleDescription,
    [System.Text.UTF8Encoding]::new($false)
)

Assert-CommonPackageFiles $slimRoot.FullName
Assert-CommonPackageFiles $compatibleRoot.FullName
$unexpectedSlimRuntime = Get-ChildItem -LiteralPath $slimRoot.FullName -File |
    Where-Object Name -Match $slimRuntimePattern |
    Select-Object -First 1
if ($unexpectedSlimRuntime) {
    throw "Slim package contains $($unexpectedSlimRuntime.Name)"
}
foreach ($name in $crtNames + $win7ShimNames + @(
    'ucrtbase.dll',
    'api-ms-win-crt-runtime-l1-1-0.dll'
)) {
    if (-not (Test-Path -LiteralPath (Join-Path $compatibleRoot.FullName $name) -PathType Leaf)) {
        throw "Compatible package is missing $name"
    }
}
Assert-CompatibleApiSetClosure $compatibleRoot.FullName $dumpbinPath

$slimArchive = Assert-ChildPath `
    (Join-Path $destination "workflow-browser-ui-$Version-windows-x64-slim.zip") `
    $destination
$compatibleArchive = Assert-ChildPath `
    (Join-Path $destination "workflow-browser-ui-$Version-windows-x64-win7-compatible.zip") `
    $destination
foreach ($archive in @($slimArchive, $compatibleArchive)) {
    if (Test-Path -LiteralPath $archive) {
        Remove-Item -LiteralPath $archive -Force
    }
    if (Test-Path -LiteralPath "$archive.sha256") {
        Remove-Item -LiteralPath "$archive.sha256" -Force
    }
}
Compress-Archive -Path (Join-Path $slimRoot.FullName '*') `
    -DestinationPath $slimArchive -CompressionLevel Optimal
Compress-Archive -Path (Join-Path $compatibleRoot.FullName '*') `
    -DestinationPath $compatibleArchive -CompressionLevel Optimal
$slimChecksum = Write-Checksum $slimArchive
$compatibleChecksum = Write-Checksum $compatibleArchive

Remove-Item -LiteralPath $workRoot -Recurse -Force

[pscustomobject] @{
    SlimArchive = $slimArchive
    SlimChecksum = $slimChecksum
    CompatibleArchive = $compatibleArchive
    CompatibleChecksum = $compatibleChecksum
    VcRuntimeDirectory = $vcRuntime
    UcrtRuntimeDirectory = $ucrtRuntime
}

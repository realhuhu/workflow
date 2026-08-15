[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $SdkDirectory,

    [Parameter(Mandatory = $true)]
    [string] $TestDirectory,

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
$testRequiredPaths = @(
    'workflow-test.exe',
    'README.md',
    'test-page\index.html',
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
$sdkRequiredPaths = @(
    'include\workflow.h',
    'lib\workflow.lib',
    'lib\workflow_rapidocr.lib',
    'lib\onnxruntime.lib',
    'lib\cmake\workflow\workflowConfig.cmake',
    'bin\onnxruntime.dll',
    'share\workflow\models\det.onnx',
    'share\workflow\models\rec.onnx',
    'share\workflow\licenses\Qt-LGPL-GPL-3.0.txt'
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

function Assert-TestPackageFiles(
    [string] $Root
) {
    foreach ($relativePath in $testRequiredPaths) {
        $path = Join-Path $Root $relativePath
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Test package is missing $relativePath"
        }
    }
    $opencvRuntime = Get-ChildItem -LiteralPath $Root -File |
        Where-Object Name -Match '^opencv_(world|core)4100\.dll$' |
        Select-Object -First 1
    if (-not $opencvRuntime) {
        throw 'Test package is missing the OpenCV 4.10 runtime'
    }
}

function Assert-SdkPackageFiles(
    [string] $Root
) {
    foreach ($relativePath in $sdkRequiredPaths) {
        $path = Join-Path $Root $relativePath
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "SDK package is missing $relativePath"
        }
    }
    foreach ($runtime in @('Qt5Core.dll', 'onnxruntime.dll')) {
        if (-not (Test-Path -LiteralPath (Join-Path $Root "bin\$runtime") -PathType Leaf)) {
            throw "SDK package is missing bin\$runtime"
        }
    }
    $opencvRuntime = Get-ChildItem -LiteralPath (Join-Path $Root 'bin') -File |
        Where-Object Name -Match '^opencv_(world|core)4100\.dll$' |
        Select-Object -First 1
    if (-not $opencvRuntime) {
        throw 'SDK package is missing the OpenCV 4.10 runtime'
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

function Write-PackageVariant(
    [string] $Root,
    [string[]] $Description
) {
    [System.IO.File]::WriteAllLines(
        (Join-Path $Root 'PACKAGE_VARIANT.txt'),
        $Description,
        [System.Text.UTF8Encoding]::new($false)
    )
}

$sdkSource = Resolve-ExistingDirectory $SdkDirectory 'SDK source directory'
$testSource = Resolve-ExistingDirectory $TestDirectory 'Test source directory'
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
    if (-not (Test-Path -LiteralPath (Join-Path $sdkSource "bin\$name") -PathType Leaf)) {
        throw "SDK source directory is missing the Win7 shim $name"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $testSource $name) -PathType Leaf)) {
        throw "Test source directory is missing the Win7 shim $name"
    }
}

$workRoot = Reset-ChildDirectory `
    (Join-Path $destination ".package-$([guid]::NewGuid().ToString('N'))") `
    $destination
$sdkSlimRoot = New-Item -ItemType Directory -Path (Join-Path $workRoot 'sdk-slim')
$sdkCompatibleRoot = New-Item -ItemType Directory -Path (Join-Path $workRoot 'sdk-compatible')
$testSlimRoot = New-Item -ItemType Directory -Path (Join-Path $workRoot 'test-slim')
$testCompatibleRoot = New-Item -ItemType Directory -Path (Join-Path $workRoot 'test-compatible')

foreach ($root in @($sdkSlimRoot, $sdkCompatibleRoot)) {
    Get-ChildItem -LiteralPath $sdkSource -Force |
        Copy-Item -Destination $root.FullName -Recurse -Force
    $bin = Join-Path $root.FullName 'bin'
    Copy-Item -LiteralPath (Join-Path $testSource 'Qt5Core.dll') -Destination $bin -Force
    Get-ChildItem -LiteralPath $testSource -File |
        Where-Object Name -Match '^opencv_.*4100\.dll$' |
        Copy-Item -Destination $bin -Force
}
foreach ($root in @($testSlimRoot, $testCompatibleRoot)) {
    Get-ChildItem -LiteralPath $testSource -Force |
        Copy-Item -Destination $root.FullName -Recurse -Force
}

$slimRuntimePattern = '^(concrt140|msvcp140.*|vcruntime140.*|vccorlib140|ucrtbase|api-ms-win-(?:crt|core|eventing).*)\.dll$'
foreach ($root in @(
    (Join-Path $sdkSlimRoot.FullName 'bin'),
    $testSlimRoot.FullName
)) {
    Get-ChildItem -LiteralPath $root -File |
        Where-Object Name -Match $slimRuntimePattern |
        ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }
}

foreach ($root in @(
    (Join-Path $sdkCompatibleRoot.FullName 'bin'),
    $testCompatibleRoot.FullName
)) {
    foreach ($name in $crtNames) {
        Copy-Item -LiteralPath (Join-Path $vcRuntime $name) -Destination $root -Force
    }
    Get-ChildItem -LiteralPath $ucrtRuntime -File -Filter '*.dll' |
        Copy-Item -Destination $root -Force
}

$sdkSlimDescription = @(
    'Package: Workflow SDK',
    'Variant: slim',
    'Requires: Windows 10 x64 and Microsoft Visual C++ 2015-2022 Redistributable x64.',
    'Contains: headers, libraries, CMake package files, Qt Core, OpenCV, ONNX Runtime, OCR models, and licenses.'
)
$sdkCompatibleDescription = @(
    'Package: Workflow SDK',
    'Variant: Windows 7 SP1 compatible',
    'Requires: Windows 7 SP1 x64 or newer.',
    'Contains: SDK slim files plus MSVC v142 CRT, app-local UCRT, and Win7 API-set forwarders.'
)
$testSlimDescription = @(
    'Package: Workflow Test',
    'Variant: slim',
    'Requires: Windows 10 x64 and Microsoft Visual C++ 2015-2022 Redistributable x64.',
    'Contains: test application, Qt, OpenCV, ONNX Runtime, OCR models, 20 test pages, and licenses.'
)
$testCompatibleDescription = @(
    'Package: Workflow Test',
    'Variant: Windows 7 SP1 compatible',
    'Requires: Windows 7 SP1 x64 or newer.',
    'Contains: test slim files plus MSVC v142 CRT, app-local UCRT, and Win7 API-set forwarders.'
)
Write-PackageVariant $sdkSlimRoot.FullName $sdkSlimDescription
Write-PackageVariant $sdkCompatibleRoot.FullName $sdkCompatibleDescription
Write-PackageVariant $testSlimRoot.FullName $testSlimDescription
Write-PackageVariant $testCompatibleRoot.FullName $testCompatibleDescription

Assert-SdkPackageFiles $sdkSlimRoot.FullName
Assert-SdkPackageFiles $sdkCompatibleRoot.FullName
Assert-TestPackageFiles $testSlimRoot.FullName
Assert-TestPackageFiles $testCompatibleRoot.FullName
foreach ($root in @(
    (Join-Path $sdkSlimRoot.FullName 'bin'),
    $testSlimRoot.FullName
)) {
    $unexpectedRuntime = Get-ChildItem -LiteralPath $root -File |
        Where-Object Name -Match $slimRuntimePattern |
        Select-Object -First 1
    if ($unexpectedRuntime) {
        throw "Slim package contains $($unexpectedRuntime.Name)"
    }
}
foreach ($root in @(
    (Join-Path $sdkCompatibleRoot.FullName 'bin'),
    $testCompatibleRoot.FullName
)) {
    foreach ($name in $crtNames + $win7ShimNames + @(
        'ucrtbase.dll',
        'api-ms-win-crt-runtime-l1-1-0.dll'
    )) {
        if (-not (Test-Path -LiteralPath (Join-Path $root $name) -PathType Leaf)) {
            throw "Compatible package is missing $name"
        }
    }
}
Assert-CompatibleApiSetClosure $sdkCompatibleRoot.FullName $dumpbinPath
Assert-CompatibleApiSetClosure $testCompatibleRoot.FullName $dumpbinPath

$sdkSlimArchive = Assert-ChildPath `
    (Join-Path $destination "workflow-sdk-$Version-windows-x64-slim.zip") `
    $destination
$sdkCompatibleArchive = Assert-ChildPath `
    (Join-Path $destination "workflow-sdk-$Version-windows-x64-win7-compatible.zip") `
    $destination
$testSlimArchive = Assert-ChildPath `
    (Join-Path $destination "workflow-test-$Version-windows-x64-slim.zip") `
    $destination
$testCompatibleArchive = Assert-ChildPath `
    (Join-Path $destination "workflow-test-$Version-windows-x64-win7-compatible.zip") `
    $destination
foreach ($archive in @(
    $sdkSlimArchive,
    $sdkCompatibleArchive,
    $testSlimArchive,
    $testCompatibleArchive
)) {
    if (Test-Path -LiteralPath $archive) {
        Remove-Item -LiteralPath $archive -Force
    }
    if (Test-Path -LiteralPath "$archive.sha256") {
        Remove-Item -LiteralPath "$archive.sha256" -Force
    }
}
Compress-Archive -Path (Join-Path $sdkSlimRoot.FullName '*') `
    -DestinationPath $sdkSlimArchive -CompressionLevel Optimal
Compress-Archive -Path (Join-Path $sdkCompatibleRoot.FullName '*') `
    -DestinationPath $sdkCompatibleArchive -CompressionLevel Optimal
Compress-Archive -Path (Join-Path $testSlimRoot.FullName '*') `
    -DestinationPath $testSlimArchive -CompressionLevel Optimal
Compress-Archive -Path (Join-Path $testCompatibleRoot.FullName '*') `
    -DestinationPath $testCompatibleArchive -CompressionLevel Optimal
$sdkSlimChecksum = Write-Checksum $sdkSlimArchive
$sdkCompatibleChecksum = Write-Checksum $sdkCompatibleArchive
$testSlimChecksum = Write-Checksum $testSlimArchive
$testCompatibleChecksum = Write-Checksum $testCompatibleArchive

Remove-Item -LiteralPath $workRoot -Recurse -Force

[pscustomobject] @{
    SdkSlimArchive = $sdkSlimArchive
    SdkSlimChecksum = $sdkSlimChecksum
    SdkCompatibleArchive = $sdkCompatibleArchive
    SdkCompatibleChecksum = $sdkCompatibleChecksum
    TestSlimArchive = $testSlimArchive
    TestSlimChecksum = $testSlimChecksum
    TestCompatibleArchive = $testCompatibleArchive
    TestCompatibleChecksum = $testCompatibleChecksum
    VcRuntimeDirectory = $vcRuntime
    UcrtRuntimeDirectory = $ucrtRuntime
}

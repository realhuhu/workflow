[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $Destination
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$qtVersion = '5.15.2'
$qtArchitecture = 'win64_msvc2019_64'
$opencvVersion = '4.10.0'
$onnxRuntimeVersion = '1.13.1'
$opencvSha256 = 'BFF38466091C313DAC21A0B73EE8278316A89C1D434C6F0B10697E087670168'
$onnxRuntimeSha256 = 'CD8318DC30352E0D615F809BD544BFD18B578289EC16621252B5DB1994F09E43'

function Get-VerifiedDownload(
    [string] $Uri,
    [string] $Path,
    [string] $Sha256
) {
    if (-not (Test-Path -LiteralPath $Path)) {
        Invoke-WebRequest -Uri $Uri -OutFile $Path
    }
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash
    if ($actual -ne $Sha256) {
        throw "SHA-256 mismatch for $Path. Expected $Sha256, found $actual"
    }
}

$destinationPath = [System.IO.Path]::GetFullPath($Destination)
$downloadsPath = Join-Path $destinationPath 'downloads'
New-Item -ItemType Directory -Force -Path $destinationPath, $downloadsPath | Out-Null

$qtInstallPath = Join-Path $destinationPath 'Qt'
$qtRoot = Join-Path $qtInstallPath "$qtVersion\msvc2019_64"
if (-not (Test-Path -LiteralPath (Join-Path $qtRoot 'bin\Qt5Core.dll'))) {
    & python -m aqt install-qt windows desktop $qtVersion $qtArchitecture -O $qtInstallPath
    if ($LASTEXITCODE -ne 0) {
        throw "aqtinstall failed with exit code $LASTEXITCODE"
    }
}

$opencvArchive = Join-Path $downloadsPath "opencv-$opencvVersion-windows.exe"
$opencvContainer = Join-Path $destinationPath "opencv-$opencvVersion"
$opencvRoot = Join-Path $opencvContainer 'opencv\build'
Get-VerifiedDownload `
    -Uri "https://github.com/opencv/opencv/releases/download/$opencvVersion/opencv-$opencvVersion-windows.exe" `
    -Path $opencvArchive `
    -Sha256 $opencvSha256
if (-not (Test-Path -LiteralPath (Join-Path $opencvRoot 'OpenCVConfig.cmake'))) {
    $process = Start-Process `
        -FilePath $opencvArchive `
        -ArgumentList "-o`"$opencvContainer`"", '-y' `
        -Wait `
        -PassThru `
        -WindowStyle Hidden
    if ($process.ExitCode -ne 0) {
        throw "OpenCV extraction failed with exit code $($process.ExitCode)"
    }
}

$onnxRuntimeArchive = Join-Path $downloadsPath "onnxruntime-win-x64-$onnxRuntimeVersion.zip"
$onnxRuntimeRoot = Join-Path $destinationPath "onnxruntime-win-x64-$onnxRuntimeVersion"
Get-VerifiedDownload `
    -Uri "https://github.com/microsoft/onnxruntime/releases/download/v$onnxRuntimeVersion/onnxruntime-win-x64-$onnxRuntimeVersion.zip" `
    -Path $onnxRuntimeArchive `
    -Sha256 $onnxRuntimeSha256
if (-not (Test-Path -LiteralPath (Join-Path $onnxRuntimeRoot 'lib\onnxruntime.dll'))) {
    Expand-Archive -LiteralPath $onnxRuntimeArchive -DestinationPath $destinationPath -Force
}

$opencvBin = Join-Path $opencvRoot 'x64\vc16\bin'
$resolved = [ordered]@{
    WORKFLOW_QT_ROOT = $qtRoot
    WORKFLOW_OPENCV_DIR = $opencvRoot
    WORKFLOW_OPENCV_BIN_DIR = $opencvBin
    WORKFLOW_ONNXRUNTIME_ROOT = $onnxRuntimeRoot
}

foreach ($entry in $resolved.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $entry.Value)) {
        throw "Dependency path does not exist: $($entry.Key)=$($entry.Value)"
    }
    Set-Item -Path "Env:$($entry.Key)" -Value $entry.Value
    if ($env:GITHUB_ENV) {
        "$($entry.Key)=$($entry.Value)" | Out-File -FilePath $env:GITHUB_ENV -Append -Encoding utf8
    }
    Write-Host "$($entry.Key)=$($entry.Value)"
}

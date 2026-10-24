# Build the unofficial vanilla-support tracker on an existing Windows C++ setup.
# Prerequisites: Git, Python 3.10+, CMake >= 3.26, and Visual Studio 2022 with
# "Desktop development with C++" (including its Windows SDK).
# This script does not install Visual Studio or accept Microsoft license terms.
[CmdletBinding()]
param(
    [string]$Generator = "Visual Studio 17 2022",
    [string]$BuildDirectory = "build-windows-vanilla"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program failed with exit code $LASTEXITCODE."
    }
}

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$GitPath = (Get-Command git -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$PythonPath = (Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$CMakePath = (Get-Command cmake -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$CTestPath = Join-Path (Split-Path -Parent $CMakePath) "ctest.exe"
if (-not (Test-Path -LiteralPath $CTestPath)) {
    throw "ctest.exe was not found next to cmake.exe. Install CMake >= 3.26."
}
if (-not [System.IO.Path]::IsPathRooted($BuildDirectory)) {
    $BuildDirectory = Join-Path $ProjectRoot $BuildDirectory
}

Push-Location $ProjectRoot
try {
    Invoke-Checked -Program $GitPath -Arguments @("--version")
    Invoke-Checked -Program $PythonPath -Arguments @("--version")
    Invoke-Checked -Program $CMakePath -Arguments @("--version")

    # Fetch only the pinned public SDK/dependencies; no game image is required.
    Invoke-Checked -Program $PythonPath -Arguments @("tools/bootstrap.py", "--with-randomizer")

    # Link against the exact user's host release, not the SDK's mutable latest stub.
    # Only the public import library is extracted; the host executable is not run.
    $HostSdkDirectory = Join-Path $BuildDirectory "dusklight-2.0.3-sdk"
    New-Item -ItemType Directory -Path $HostSdkDirectory -Force | Out-Null
    $HostArchive = Join-Path $HostSdkDirectory "Dusklight-v2.0.3-win32-x86_64.zip"
    $HostArchiveHash = "ac63514eeb13cd4e9bd22be02dabee4952d1331a78f0496326bba2b577b12caf"
    if (-not (Test-Path -LiteralPath $HostArchive)) {
        Invoke-WebRequest -UseBasicParsing -Uri (
            "https://github.com/TwilitRealm/dusklight/releases/download/v2.0.3/" +
            "Dusklight-v2.0.3-win32-x86_64.zip"
        ) -OutFile $HostArchive
    }
    if ((Get-FileHash -LiteralPath $HostArchive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $HostArchiveHash) {
        throw "Dusklight v2.0.3 download checksum does not match its official release digest."
    }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $HostLibrary = Join-Path $HostSdkDirectory "windows-amd64.lib"
    $HostZip = [System.IO.Compression.ZipFile]::OpenRead($HostArchive)
    try {
        $HostLibraryEntry = $HostZip.GetEntry("sdk/windows-amd64.lib")
        if ($null -eq $HostLibraryEntry) {
            throw "The official Dusklight package does not contain its Windows import library."
        }
        [System.IO.Compression.ZipFileExtensions]::ExtractToFile($HostLibraryEntry, $HostLibrary, $true)
    }
    finally {
        $HostZip.Dispose()
    }
    Invoke-Checked -Program $CMakePath -Arguments @(
        "-S", $ProjectRoot, "-B", $BuildDirectory,
        "-G", $Generator, "-A", "x64", "-DBUILD_TESTING=ON", "-DDUSK_GAME_EXE=$HostLibrary"
    )
    Invoke-Checked -Program $CMakePath -Arguments @(
        "--build", $BuildDirectory, "--config", "Release", "--parallel", "3"
    )
    Invoke-Checked -Program $CTestPath -Arguments @(
        "--test-dir", $BuildDirectory, "-C", "Release", "--output-on-failure"
    )
    Invoke-Checked -Program $PythonPath -Arguments @("tests/asset_tests.py")

    # Use the same pinned metadata verifier and checksum as the upstream SDK.
    $SymgenDirectory = Join-Path $BuildDirectory "package-tools"
    New-Item -ItemType Directory -Path $SymgenDirectory -Force | Out-Null
    $SymgenPath = Join-Path $SymgenDirectory "symgen-1.3.6.exe"
    $ExpectedSymgenHash = "171995a33bfd5a5271b5bb78d3f2c1d4aec58721bf3a894aaad5607ff648e2a4"
    if (-not (Test-Path -LiteralPath $SymgenPath)) {
        $DownloadPath = "$SymgenPath.download"
        Invoke-WebRequest -UseBasicParsing -Uri (
            "https://github.com/encounter/symgen/releases/download/v1.3.6/" +
            "symgen-windows-x86_64.exe"
        ) -OutFile $DownloadPath
        if ((Get-FileHash -LiteralPath $DownloadPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $ExpectedSymgenHash) {
            throw "Downloaded symgen checksum does not match the pinned SDK checksum."
        }
        Move-Item -LiteralPath $DownloadPath -Destination $SymgenPath
    }
    if ((Get-FileHash -LiteralPath $SymgenPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $ExpectedSymgenHash) {
        throw "Existing symgen checksum does not match the pinned SDK checksum."
    }

    $Metadata = Get-Content -LiteralPath (Join-Path $ProjectRoot "mod.json") -Raw | ConvertFrom-Json
    $PackagePath = Join-Path $BuildDirectory "mods/TP_Randomizer_Tracker.dusk"
    $DistributionPath = Join-Path $ProjectRoot ("dist/TPTracker-" + $Metadata.version + "-windows-amd64.dusk")
    Invoke-Checked -Program $PythonPath -Arguments @(
        ".deps/dusklight/mods/randomizer/tools/merge_mod.py",
        "--symgen", $SymgenPath, "-o", $DistributionPath, $PackagePath
    )
    $PackageHash = (Get-FileHash -LiteralPath $DistributionPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $PackageFilename = Split-Path -Leaf $DistributionPath
    [System.IO.File]::WriteAllText("$DistributionPath.sha256", "$PackageHash  $PackageFilename`n")
    $SourceRevision = "source-archive"
    if ($env:GITHUB_SHA) {
        $SourceRevision = $env:GITHUB_SHA
    }
    elseif (Test-Path -LiteralPath (Join-Path $ProjectRoot ".git")) {
        $SourceRevision = (& $GitPath rev-parse HEAD).Trim()
        if ($LASTEXITCODE -ne 0) { throw "Could not record the source revision." }
    }
    $DependencyPins = Get-Content -LiteralPath (Join-Path $ProjectRoot "dependencies.json") -Raw | ConvertFrom-Json
    $BuildInfo = [ordered]@{
        package = $PackageFilename
        sha256 = $PackageHash
        source_revision = $SourceRevision
        target_dusklight = "2.0.3"
        target_host_revision = "40457c6adb381928e4b5fef6ed459ed291edd5e2"
        target_host_archive_sha256 = $HostArchiveHash
        sdk_revision = $DependencyPins.revision
        platform = "windows-amd64"
        built_at_utc = [DateTime]::UtcNow.ToString("o")
        validation = @("CTest passed", "asset tests passed", "symgen native metadata verified")
        gameplay_tested = $false
    }
    $BuildInfoPath = Join-Path (Split-Path -Parent $DistributionPath) "BUILD_INFO.json"
    [System.IO.File]::WriteAllText($BuildInfoPath, ($BuildInfo | ConvertTo-Json -Depth 4))
    $InstructionsPath = Join-Path $ProjectRoot "NORMAL_GAME_KO.md"
    if (Test-Path -LiteralPath $InstructionsPath) {
        Copy-Item -LiteralPath $InstructionsPath -Destination (Join-Path (Split-Path -Parent $DistributionPath) "NORMAL_GAME_KO.md")
    }
    Write-Host ""
    Write-Host "Windows x64 package created: $DistributionPath"
    Write-Host "Install it through Dusklight's mod installation workflow."
    Write-Host "Runtime validation with your vanilla save is still required."
    Get-FileHash -LiteralPath $DistributionPath -Algorithm SHA256 | Format-List
}
finally {
    Pop-Location
}

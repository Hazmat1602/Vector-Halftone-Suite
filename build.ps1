param(
    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$common = Join-Path $projectRoot '..\common\source\Plugin.cpp'
$illustratorApi = Join-Path $projectRoot '..\..\illustratorapi\illustrator\AIArt.h'

if (!(Test-Path $common) -or !(Test-Path $illustratorApi)) {
    throw @"
This folder must live directly inside the Illustrator SDK's samplecode folder.
Expected layout:
  <Illustrator SDK>\samplecode\Vector-Halftone-Suite-main\build.ps1
  <Illustrator SDK>\samplecode\common\...
  <Illustrator SDK>\illustratorapi\...
"@
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path $vswhere)) {
    throw @"
Visual Studio / Build Tools was not found.
Install Visual Studio 2022 or 2026 with:
  - Desktop development with C++
  - MSVC v143 - VS 2022 C++ x64/x86 build tools
  - A Windows 10/11 SDK
"@
}

# Do not simply pick the newest MSBuild installation. A machine can have a newer
# Visual Studio instance with only .NET tooling installed, while an older
# instance contains the C++ targets we actually need. That produces MSB4019 for
# Microsoft.Cpp.Default.props before the project can even be evaluated.
$instances = @(& $vswhere -all -products * -format json | ConvertFrom-Json)
if ($instances.Count -eq 0) {
    throw 'No Visual Studio installations were found by vswhere.'
}

$cppCapable = @()
$v143Capable = @()

foreach ($instance in $instances) {
    $installPath = [string]$instance.installationPath
    if ([string]::IsNullOrWhiteSpace($installPath)) { continue }

    $msbuild = Join-Path $installPath 'MSBuild\Current\Bin\MSBuild.exe'
    $vcRoot = Join-Path $installPath 'MSBuild\Microsoft\VC'

    if (!(Test-Path $msbuild) -or !(Test-Path $vcRoot)) { continue }

    $cppProps = Get-ChildItem -Path $vcRoot -Recurse -Filter 'Microsoft.Cpp.Default.props' -File -ErrorAction SilentlyContinue |
        Select-Object -First 1

    if (!$cppProps) { continue }

    $cppCapable += [pscustomobject]@{
        Instance = $instance
        MSBuild = $msbuild
        VCRoot = $vcRoot
    }

    $v143Props = Get-ChildItem -Path $vcRoot -Recurse -Filter 'Toolset.props' -File -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '[\\/]PlatformToolsets[\\/]v143[\\/]' } |
        Select-Object -First 1

    if ($v143Props) {
        $v143Capable += [pscustomobject]@{
            Instance = $instance
            MSBuild = $msbuild
            VCRoot = $vcRoot
            V143 = $v143Props.FullName
        }
    }
}

if ($cppCapable.Count -eq 0) {
    throw @"
Visual Studio is installed, but no installation contains the Visual C++ MSBuild targets.

Open Visual Studio Installer -> Modify and install:
  Workload:
    Desktop development with C++

Then make sure a Windows 10 or Windows 11 SDK is selected.
"@
}

if ($v143Capable.Count -eq 0) {
    $found = ($cppCapable | ForEach-Object { $_.Instance.displayName }) -join ', '
    throw @"
C++ tooling was found ($found), but the v143 platform toolset required by the Illustrator project is missing.

Open Visual Studio Installer -> Modify -> Individual components and install:
  MSVC v143 - VS 2022 C++ x64/x86 build tools

If you are using Visual Studio 2026, v143 is an optional compatibility toolset and
must be selected separately from the latest C++ tools.
"@
}

$selected = $v143Capable |
    Sort-Object { [version]$_.Instance.installationVersion } -Descending |
    Select-Object -First 1

$msbuild = $selected.MSBuild
Write-Host ("Using {0} ({1})" -f $selected.Instance.displayName, $selected.Instance.installationVersion) -ForegroundColor Cyan
Write-Host ("MSBuild: {0}" -f $msbuild)

Push-Location $projectRoot
try {
    & $msbuild '.\VectorHalftoneEffect.sln' /t:Rebuild /m /p:Configuration=$Configuration /p:Platform=x64
    if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }

    $plugin = Resolve-Path "..\output\win\x64\$Configuration\VectorHalftoneEffect.aip"
    Write-Host "`nBuilt successfully:" -ForegroundColor Green
    Write-Host $plugin
} finally {
    Pop-Location
}

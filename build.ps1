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
  <Illustrator SDK>\samplecode\VectorHalftoneEffect\build.ps1
  <Illustrator SDK>\samplecode\common\...
  <Illustrator SDK>\illustratorapi\...
"@
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path $vswhere)) {
    throw 'Visual Studio 2022 / Build Tools was not found. Install Desktop development with C++ first.'
}

$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'MSBuild was not found.' }

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

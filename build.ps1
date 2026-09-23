param(
    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Release',
    [string]$MSBuildPath
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

# Use the caller's selected build environment before automatic discovery.
$msbuild = $null
if ($MSBuildPath) {
    $msbuild = (Resolve-Path -LiteralPath $MSBuildPath).Path
    if (!(Test-Path -LiteralPath $msbuild -PathType Leaf)) {
        throw "MSBuildPath must point to MSBuild.exe: $MSBuildPath"
    }
} else {
    $onPath = Get-Command MSBuild.exe -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($onPath) { $msbuild = $onPath.Source }

    if (!$msbuild) {
        $installerRoot = [Environment]::GetFolderPath('ProgramFilesX86')
        $vswhere = Join-Path $installerRoot 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
            # Prefer C++ installations, but never block a build based on
            # installer metadata or a particular Toolset.props folder layout.
            $queries = @(
                @('-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64'),
                @('-requires', 'Microsoft.Component.MSBuild')
            )
            foreach ($query in $queries) {
                $found = @(& $vswhere -latest -prerelease -products '*' @query -find 'MSBuild\**\Bin\MSBuild.exe')
                if ($LASTEXITCODE -ne 0) { continue }
                $msbuild = $found | Where-Object {
                    $_ -and (Test-Path -LiteralPath $_ -PathType Leaf)
                } | Select-Object -First 1
                if ($msbuild) { break }
            }
        }
    }

    # Fallback when vswhere is unavailable or its metadata is incomplete.
    if (!$msbuild) {
        $roots = @($env:ProgramFiles, [Environment]::GetFolderPath('ProgramFilesX86')) |
            Where-Object { $_ } | Select-Object -Unique
        foreach ($root in $roots) {
            $pattern = Join-Path $root 'Microsoft Visual Studio\*\*\MSBuild\Current\Bin\MSBuild.exe'
            $candidate = Get-ChildItem -Path $pattern -File -ErrorAction SilentlyContinue |
                Sort-Object LastWriteTime -Descending | Select-Object -First 1
            if ($candidate) { $msbuild = $candidate.FullName; break }
        }
    }
}

if (!$msbuild) {
    throw @"
MSBuild.exe could not be located. Run from Developer PowerShell,
or supply its location:
  .\build.ps1 -MSBuildPath 'C:\path\to\MSBuild.exe'
"@
}

Write-Host ("MSBuild: {0}" -f $msbuild) -ForegroundColor Cyan
$logPath = Join-Path $projectRoot 'build-log.txt'
$pluginPath = [IO.Path]::GetFullPath(
    (Join-Path $projectRoot "..\output\win\x64\$Configuration\VectorHalftoneEffect.aip"))
Write-Host ("Build log: {0}" -f $logPath)
Write-Host ("Expected plug-in: {0}" -f $pluginPath)

Push-Location $projectRoot
try {
    $buildArguments = @(
        '.\VectorHalftoneEffect.sln',
        '/t:Rebuild',
        '/m',
        "/p:Configuration=$Configuration",
        '/p:Platform=x64',
        '/fl',
        "/flp:LogFile=$logPath;Verbosity=normal;Encoding=UTF-8"
    )
    & $msbuild @buildArguments
    $buildExitCode = $LASTEXITCODE
    if ($buildExitCode -ne 0) {
        throw "Build failed with exit code $buildExitCode. See MSBuild's error above or $logPath"
    }
    if (!(Test-Path -LiteralPath $pluginPath -PathType Leaf)) {
        throw "MSBuild succeeded but the expected plug-in was not found: $pluginPath"
    }
    Write-Host 'Built successfully:' -ForegroundColor Green
    Write-Host $pluginPath
} finally {
    Pop-Location
}

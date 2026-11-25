[CmdletBinding()]
param(
    [string]$JucePath = 'D:\JUCE',
    [string]$BuildDir = 'build-vs18-alpha',
    [string]$Iscc,
    [string]$Rar,
    [switch]$PluginOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$version = '2.1.1-alpha'
$bundleName = 'Skeuomorphic Space Panner 2.1.1 Alpha.vst3'
$cmake = (Get-Command cmake -CommandType Application).Source

function Invoke-BuildTool {
    param([string]$Path, [string[]]$Arguments)
    & $Path @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Path exited with code $LASTEXITCODE."
    }
}

function Find-BuildTool {
    param([string]$Specified, [string]$Name, [string[]]$Candidates)
    if ($Specified) {
        return (Get-Item -LiteralPath $Specified -ErrorAction Stop).FullName
    }
    $installedCommand = Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue
    if ($installedCommand) { return $installedCommand.Source }
    foreach ($candidate in $Candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    throw "Install $Name or pass its full path with the script parameter."
}

if (-not (Test-Path -LiteralPath (Join-Path $JucePath 'CMakeLists.txt') -PathType Leaf)) {
    throw "JUCE source tree not found at $JucePath. Pass -JucePath."
}
if (-not [IO.Path]::IsPathRooted($BuildDir)) { $BuildDir = Join-Path $root $BuildDir }
$configure = @('-S', $root, '-B', $BuildDir, '-G', 'Visual Studio 18 2026', '-A', 'x64',
               "-DGETT1_JUCE_PATH=$JucePath")
Invoke-BuildTool $cmake $configure
Invoke-BuildTool $cmake @('--build', $BuildDir, '--config', 'Release', '--parallel', '2', '--target', 'GettoneSpacePanner_VST3')
$bundle = Join-Path $BuildDir ("GettoneSpacePanner_artefacts\Release\VST3\" + $bundleName)
$binary = Join-Path $bundle ("Contents\x86_64-win\" + $bundleName)
if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) { throw "Windows x64 VST3 missing: $binary" }
Write-Host "VST3: $bundle"
if ($PluginOnly) { return }

$isccCommand = Find-BuildTool $Iscc 'ISCC.exe' @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe")
$rarCommand = Find-BuildTool $Rar 'Rar.exe' @(
    "$env:ProgramFiles\WinRAR\Rar.exe",
    "${env:ProgramFiles(x86)}\WinRAR\Rar.exe")

$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Path $dist -Force | Out-Null
$stage = Join-Path $dist ("stage-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
try {
    $innoArgs = @("/DVersion=$version", "/DBundleDir=$bundle", "/DOutputFolder=$stage")
    $innoArgs += (Join-Path $PSScriptRoot 'installer.iss')
    Invoke-BuildTool $isccCommand $innoArgs
    Copy-Item -LiteralPath (Join-Path $root 'Manuali') -Destination $stage -Recurse
    Copy-Item -LiteralPath (Join-Path $root 'LEGGIMI.txt') -Destination $stage
    Copy-Item -LiteralPath (Join-Path $root 'Assets\OFL.txt') -Destination (Join-Path $stage 'PixelifySans-OFL.txt')
    $installer = 'Skeuomorphic_Space_Panner_Alpha_Win_Installer.exe'
    if (-not (Test-Path -LiteralPath (Join-Path $stage $installer) -PathType Leaf)) {
        throw 'Installer output missing.'
    }
    $archiveName = "Skeuomorphic_Space_Panner_${version}_Windows_x64.rar"
    $newArchive = Join-Path $dist ("stage-" + [Guid]::NewGuid().ToString('N') + '.rar')
    $finalArchive = Join-Path $dist $archiveName
    try {
        Push-Location $stage
        try {
            Invoke-BuildTool $rarCommand @('a', '-r', '-m5', '-idq', $newArchive,
                                          $installer, 'Manuali', 'LEGGIMI.txt', 'PixelifySans-OFL.txt')
            Invoke-BuildTool $rarCommand @('t', $newArchive)
        } finally {
            Pop-Location
        }
        Move-Item -LiteralPath $newArchive -Destination $finalArchive -Force
    } finally {
        if (Test-Path -LiteralPath $newArchive) { Remove-Item -LiteralPath $newArchive -Force }
    }
    Copy-Item -LiteralPath (Join-Path $stage $installer) -Destination $dist -Force
    Write-Host "Gumroad: $finalArchive"
} finally {
    Remove-Item -LiteralPath $stage -Recurse -Force
}

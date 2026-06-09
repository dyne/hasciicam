param(
    [Parameter(Mandatory = $false)]
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\..\build'),

    [Parameter(Mandatory = $false)]
    [string]$StageDir = (Join-Path $PSScriptRoot '..\..\build\installer-stage'),

    [Parameter(Mandatory = $false)]
    [string]$OutputDir = (Join-Path $PSScriptRoot '..\..\releases'),

    [Parameter(Mandatory = $false)]
    [string]$IsccPath = 'C:\Program Files\Inno Setup 7\ISCC.exe',

    [Parameter(Mandatory = $false)]
    [string]$Version
)

$ErrorActionPreference = 'Stop'

function Get-RepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}

function Get-CMakeProjectVersion {
    param([string]$CachePath)

    if (-not (Test-Path $CachePath)) {
        throw "Missing CMake cache: $CachePath"
    }
    $match = Select-String -Path $CachePath -Pattern '^CMAKE_PROJECT_VERSION:STATIC=(.+)$' | Select-Object -First 1
    if (-not $match) {
        throw "Unable to determine project version from $CachePath"
    }
    return $match.Matches[0].Groups[1].Value.Trim()
}

function Ensure-EmptyDirectory {
    param([string]$Path)

    if (Test-Path $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Path -Force | Out-Null
}

function Copy-Tree {
    param(
        [string]$Source,
        [string]$Destination
    )

    if (-not (Test-Path $Source)) {
        throw "Missing source path: $Source"
    }
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    Copy-Item -Path (Join-Path $Source '*') -Destination $Destination -Recurse -Force
}

function Copy-FileIfPresent {
    param(
        [string]$Source,
        [string]$DestinationDirectory
    )

    if (Test-Path $Source) {
        New-Item -ItemType Directory -Path $DestinationDirectory -Force | Out-Null
        Copy-Item -LiteralPath $Source -Destination $DestinationDirectory -Force
    }
}

function Invoke-Checked {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$WorkingDirectory
    )

    Push-Location $WorkingDirectory
    try {
        & $FilePath @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "Command failed ($LASTEXITCODE): $FilePath $($Arguments -join ' ')"
        }
    } finally {
        Pop-Location
    }
}

$RepoRoot = Get-RepoRoot
if (-not (Test-Path $BuildDir)) {
    throw "Build directory not found: $BuildDir"
}

if (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot $BuildDir
}
if (-not [System.IO.Path]::IsPathRooted($StageDir)) {
    $StageDir = Join-Path $RepoRoot $StageDir
}
if (-not [System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = Join-Path $RepoRoot $OutputDir
}

$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)
$StageDir = [System.IO.Path]::GetFullPath($StageDir)
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)

if (-not $Version) {
    $Version = Get-CMakeProjectVersion -CachePath (Join-Path $BuildDir 'CMakeCache.txt')
}

if (-not (Test-Path $IsccPath)) {
    throw "Inno Setup compiler not found: $IsccPath"
}

Ensure-EmptyDirectory -Path $StageDir
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

Invoke-Checked -FilePath 'cmake' -Arguments @('--install', $BuildDir, '--prefix', $StageDir, '--config', 'Release') -WorkingDirectory $RepoRoot

Copy-FileIfPresent -Source (Join-Path $RepoRoot 'README.md') -DestinationDirectory $StageDir
Copy-FileIfPresent -Source (Join-Path $RepoRoot 'COPYING') -DestinationDirectory $StageDir
Copy-FileIfPresent -Source (Join-Path $RepoRoot 'docs\windows-installer.md') -DestinationDirectory (Join-Path $StageDir 'docs')
Copy-Tree -Source (Join-Path $RepoRoot 'LICENSES') -Destination (Join-Path $StageDir 'licenses')
Copy-FileIfPresent -Source (Join-Path $StageDir 'share\man\man1\hasciicam.1') -DestinationDirectory (Join-Path $StageDir 'docs')

Get-ChildItem -Path $BuildDir -Filter '*.dll' -File | ForEach-Object {
    if ($_.Name -ne 'hasciicam_virtual_camera_source.dll') {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $StageDir 'bin') -Force
    }
}

$requiredStageFiles = @(
    (Join-Path $StageDir 'bin\hasciicam.exe'),
    (Join-Path $StageDir 'bin\hasciicam_vcamctl.exe'),
    (Join-Path $StageDir 'bin\hasciicam_virtual_camera_source.dll'),
    (Join-Path $StageDir 'README.md'),
    (Join-Path $StageDir 'COPYING'),
    (Join-Path $StageDir 'docs\hasciicam.1'),
    (Join-Path $StageDir 'docs\windows-installer.md'),
    (Join-Path $StageDir 'licenses')
)

foreach ($requiredPath in $requiredStageFiles) {
    if (-not (Test-Path $requiredPath)) {
        throw "Required staging path missing: $requiredPath"
    }
}

$HasSDL2Dll = Test-Path (Join-Path $StageDir 'bin\SDL2.dll')

$issPath = Join-Path $PSScriptRoot 'hasciicam.iss'
$defines = @(
    "/DMyAppVersion=$Version",
    "/DMyBuildHome=$StageDir",
    "/DMyOutputDir=$OutputDir"
)
if ($HasSDL2Dll) {
    $defines += '/DHasSDL2Dll=1'
} else {
    $defines += '/DHasSDL2Dll=0'
}

Invoke-Checked -FilePath $IsccPath -Arguments ($defines + @($issPath)) -WorkingDirectory $RepoRoot

Write-Host "Installer built in $OutputDir"

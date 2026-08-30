<#
.SYNOPSIS
    Builds, stages and packages the Windows MIDI Services Bluetooth MIDI preview installer.

.DESCRIPTION
    The Bluetooth preview is a single installer that spans both release trains: the transport
    comes out of src/in-box/Midi2.sln like the other service plugins, and the Bluetooth MIDI Setup
    app plus its app-local copy of the API come out of src/in-box/Midi2-AppSDK.sln. That is why it
    has its own script rather than being another entry in build-plugins.ps1.

    Version numbers come from build/version-plugins.json and are written to
    build/staging/version/BundleInfo.wxi, exactly as build-plugins.ps1 writes them. The two
    scripts generate identical content from the same source file, so running either is safe.

    The bundle carries the Visual C++ runtime and the Windows App Runtime, because a customer
    installing only the Bluetooth preview will not have the App SDK runtime installer that
    normally provides them.

.PARAMETER Target
    One or more of:
      Version  Compute versions and write BundleInfo.wxi.
      Service  Build src/in-box/Midi2.sln for each platform (the transport).
      App      Build src/in-box/Midi2-AppSDK.sln for each platform (the setup app and the SDK).
      Stage    Copy the transport into build/staging/api, and the app payload into
               build/staging/bluetooth-app.
      Setup    Build the installer.
      Release  Collect the installer into build/release/bluetooth-<version>.
      Clean    Delete the Bluetooth staging folders.
      All      Version, Service, App, Stage, Setup, Release.

.PARAMETER BuildNumber
    Overrides the 'build' field in version-plugins.json without modifying the file. For CI.

.PARAMETER BumpBuildNumber
    Increments and persists the 'build' field in version-plugins.json before computing versions.

.EXAMPLE
    .\build-bluetooth.ps1
    Full Bluetooth preview build for x64 and Arm64.

.EXAMPLE
    .\build-bluetooth.ps1 -Target Setup -Platform x64
    Rebuild just the installer for x64 against whatever is already staged.
#>
[CmdletBinding()]
param(
    # Comma-separated is accepted as a single token so this works through `pwsh -File`
    # and build-bluetooth.cmd, which do not split array arguments.
    [string[]] $Target = @('All'),

    [string[]] $Platform = @('x64', 'Arm64'),

    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Release',

    [int] $BuildNumber = -1,

    [switch] $BumpBuildNumber,

    # Explicit MSBuild.exe. Leave empty to let vswhere find the newest install.
    [string] $MSBuildPath,

    [ValidateSet('quiet', 'minimal', 'normal', 'detailed', 'diagnostic')]
    [string] $Verbosity = 'minimal'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$InformationPreference = 'Continue'

function Expand-Argument {
    param([string[]] $Value, [string[]] $Allowed, [string] $Name)

    $expanded = $Value | ForEach-Object { $_ -split ',' } | Where-Object { $_ } | ForEach-Object { $_.Trim() }

    $resolved = foreach ($item in $expanded) {
        $match = $Allowed | Where-Object { $_ -eq $item }
        if (-not $match) {
            throw "Invalid -$Name value '$item'. Valid values: $($Allowed -join ', ')"
        }
        $match
    }

    , @($resolved)
}

$Target = Expand-Argument -Value $Target -Allowed @('All', 'Version', 'Service', 'App', 'Stage', 'Setup', 'Release', 'Clean') -Name 'Target'
$Platform = Expand-Argument -Value $Platform -Allowed @('x64', 'Arm64') -Name 'Platform'

# ----------------------------------------------------------------------------------------------
# Paths
# ----------------------------------------------------------------------------------------------

$BuildRoot = $PSScriptRoot
$RepoRoot = Split-Path -Parent $BuildRoot

$SourceRoot = Join-Path $RepoRoot 'src'
$ApiRoot = Join-Path $SourceRoot 'in-box'

$ServiceSolution = Join-Path $ApiRoot 'Midi2.sln'
$ServiceOutRoot = Join-Path $ApiRoot 'VSFiles'

$AppSdkSolution = Join-Path $ApiRoot 'Midi2-AppSDK.sln'
$AppSdkOutRoot = Join-Path $ApiRoot 'vsfiles-sdk\out'

$StagingRoot = Join-Path $BuildRoot 'staging'
$ReleaseRoot = Join-Path $BuildRoot 'release'
$ApiStagingRoot = Join-Path $StagingRoot 'api'
$AppStagingRoot = Join-Path $StagingRoot 'bluetooth-app'
$VersionStagingFolder = Join-Path $StagingRoot 'version'

# Shared with build-plugins.ps1. Both write the same content from version-plugins.json.
$BundleInfoFile = Join-Path $VersionStagingFolder 'BundleInfo.wxi'

$VersionFile = Join-Path $BuildRoot 'version-plugins.json'

$SetupSolutionDir = Join-Path $SourceRoot 'installers\oob-setup-bluetooth'
$SetupSolution = Join-Path $SetupSolutionDir 'midi-services-bluetooth-midi-preview-setup.sln'

$BundleName = 'WindowsMidiServicesBluetoothMidiSetup'
$InstallerName = 'Windows MIDI Services (Bluetooth MIDI Preview)'

# The .wxs files resolve staging as "$(env.MIDI_REPO_ROOT)\build\staging", so this must NOT
# have a trailing separator.
$env:MIDI_REPO_ROOT = $RepoRoot.TrimEnd('\')

$TransportBinary = 'Midi2.BluetoothMidiTransport'

# The app payload, as referenced by src/installers/oob-setup-bluetooth/api-package/WindowsMidiServices.wxs.
# The .exp, .lib and .pdb files in the same output folder are deliberately not shipped; the two
# pdbs alone are over 160 MB.
$AppPayload = @(
    'midibluetoothsetup.exe'
    'midibluetoothsetup.winmd'
    'App.xbf'
    'MainWindow.xbf'
    'resources.pri'
    'Microsoft.WindowsAppRuntime.Bootstrap.dll'
    'Microsoft.Web.WebView2.Core.dll'
    'Microsoft.Web.WebView2.Core.winmd'
    'MidiAppShared.winmd'
    'Windows.Devices.Midi2.dll'
    'Windows.Devices.Midi2.winmd'
    'Windows.Devices.Midi2.pri'
)

# ----------------------------------------------------------------------------------------------
# Output helpers
# ----------------------------------------------------------------------------------------------

function Write-Step {
    param([string] $Message)

    Write-Host ''
    Write-Host "==== $Message " -ForegroundColor Cyan -NoNewline
    Write-Host ('=' * [Math]::Max(0, 78 - $Message.Length)) -ForegroundColor Cyan
}

function Write-Detail {
    param([string] $Message)
    Write-Host "     $Message" -ForegroundColor DarkGray
}

function Write-Note {
    param([string] $Message)
    Write-Host "     $Message" -ForegroundColor Yellow
}

# ----------------------------------------------------------------------------------------------
# Version
# ----------------------------------------------------------------------------------------------

function Get-BuildVersion {
    if (-not (Test-Path $VersionFile)) {
        throw "Version file not found: $VersionFile"
    }

    $json = Get-Content $VersionFile -Raw | ConvertFrom-Json

    if ($BumpBuildNumber) {
        $json.build = [int]$json.build + 1
        $json | ConvertTo-Json -Depth 8 | Set-Content $VersionFile -Encoding UTF8
        Write-Detail "Bumped build number to $($json.build) in version-plugins.json"
    }

    $effectiveBuild = if ($BuildNumber -ge 0) { $BuildNumber } else { [int]$json.build }

    $majorMinorPatch = '{0}.{1}.{2}' -f $json.major, $json.minor, $json.patch

    if ($json.channel -eq 'stable') {
        $semVer = $majorMinorPatch
    }
    else {
        $semVer = '{0}-{1}.{2}' -f $majorMinorPatch, $json.channel, $json.channelNumber
    }

    [pscustomobject]@{
        MajorMinorPatch = $majorMinorPatch
        SemVer          = $semVer
        NumericVersion  = '{0}.{1}' -f $majorMinorPatch, $effectiveBuild
        VersionName     = [string]$json.versionName
        ReleaseLabel    = ($semVer -replace '[^\w\.\-]', '-')
    }
}

function Invoke-VersionTarget {
    param($Version)

    Write-Step 'Version'

    Write-Detail "Name            $($Version.VersionName)"
    Write-Detail "SemVer          $($Version.SemVer)"
    Write-Detail "Numeric         $($Version.NumericVersion)"

    New-Item -ItemType Directory -Force -Path $VersionStagingFolder | Out-Null

    # SetupVersionNumber is the Burn bundle version. Burn compares it as a semantic version, so
    # the prerelease tag is kept here rather than using the numeric form.
    $bundleInfo = @"
<?xml version="1.0" encoding="utf-8"?>
<!-- Generated from build\version-plugins.json by build-plugins.ps1 or build-bluetooth.ps1. Do not edit. -->
<Include>
  <?define SetupVersionName="$($Version.VersionName)" ?>
  <?define SetupVersionNumber="$($Version.SemVer)" ?>
  <?define MidiSdkAndToolsVersion="$($Version.SemVer)" ?>
  <?define MidiPluginsNumericVersion="$($Version.NumericVersion)" ?>
</Include>
"@
    Set-Content -Path $BundleInfoFile -Value $bundleInfo -Encoding UTF8
    Write-Detail "Wrote $BundleInfoFile"
}

# ----------------------------------------------------------------------------------------------
# Tool discovery
# ----------------------------------------------------------------------------------------------

function Resolve-MSBuild {
    if ($MSBuildPath) {
        if (-not (Test-Path $MSBuildPath)) { throw "MSBuild not found at -MSBuildPath: $MSBuildPath" }
        return $MSBuildPath
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found at $vswhere. Pass -MSBuildPath explicitly."
    }

    $found = & $vswhere -latest -prerelease -products * `
        -requires Microsoft.Component.MSBuild `
        -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1

    if (-not $found) {
        throw 'Could not locate MSBuild.exe via vswhere. Pass -MSBuildPath explicitly.'
    }

    return $found
}

function Invoke-MSBuild {
    param(
        [Parameter(Mandatory)] [string] $ProjectOrSolution,
        [Parameter(Mandatory)] [string] $BuildPlatform,
        [string[]] $Targets = @(),
        [hashtable] $Properties = @{},
        [string] $SolutionDir,
        [switch] $Serial
    )

    $msbuildArgs = @(
        $ProjectOrSolution
        "/p:Configuration=$Configuration"
        "/p:Platform=$BuildPlatform"
        "/v:$Verbosity"
        '/nologo'
        '/nr:false'
    )

    $msbuildArgs += if ($Serial) { '/m:1' } else { '/m' }

    if ($Targets.Count -gt 0) { $msbuildArgs += "/t:$($Targets -join ';')" }

    # Several vcxproj files include headers relative to $(SolutionDir); building without it
    # fails with C1083 on MidiDefs.h.
    if ($SolutionDir) { $msbuildArgs += "/p:SolutionDir=$($SolutionDir.TrimEnd('\'))\" }

    foreach ($key in $Properties.Keys) { $msbuildArgs += "/p:$key=$($Properties[$key])" }

    Write-Detail "msbuild $(Split-Path -Leaf $ProjectOrSolution) [$Configuration|$BuildPlatform]"

    & $script:MSBuild @msbuildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed ($LASTEXITCODE): $ProjectOrSolution [$Configuration|$BuildPlatform]"
    }
}

# ----------------------------------------------------------------------------------------------
# Build
# ----------------------------------------------------------------------------------------------

function Invoke-ServiceTarget {
    Write-Step 'Build service and transports'

    foreach ($plat in $Platform) {
        Invoke-MSBuild -ProjectOrSolution $ServiceSolution -BuildPlatform $plat `
            -Properties @{ 'NoWarn' = 'MIDL2111' }
    }
}

function Invoke-AppTarget {
    Write-Step 'Build App SDK and setup app'

    foreach ($plat in $Platform) {
        Invoke-MSBuild -ProjectOrSolution $AppSdkSolution -BuildPlatform $plat `
            -Properties @{ 'NoWarn' = 'MIDL2111' }
    }
}

# ----------------------------------------------------------------------------------------------
# Staging
# ----------------------------------------------------------------------------------------------

function Copy-Staged {
    param(
        [Parameter(Mandatory)] [string] $Source,
        [Parameter(Mandatory)] [string] $Destination
    )

    if (-not (Test-Path $Source)) { throw "Staging source not found: $Source" }

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Copy-Item -Path $Source -Destination $Destination -Force
}

function Invoke-StageTarget {
    Write-Step 'Stage'

    foreach ($plat in $Platform) {
        $transportSource = Join-Path $ServiceOutRoot "$plat\$Configuration"
        $transportDestination = Join-Path $ApiStagingRoot $plat

        foreach ($ext in @('dll', 'pdb')) {
            Copy-Staged -Source (Join-Path $transportSource "$TransportBinary.$ext") -Destination $transportDestination
        }

        Write-Detail "Staged $TransportBinary -> api\$plat"

        $appSource = Join-Path $AppSdkOutRoot "midibluetoothsetup\$plat\$Configuration"
        $appDestination = Join-Path $AppStagingRoot $plat

        # A stale file here would be silently packaged, so the folder is emptied rather than
        # copied over.
        if (Test-Path $appDestination) { Remove-Item $appDestination -Recurse -Force }

        foreach ($file in $AppPayload) {
            Copy-Staged -Source (Join-Path $appSource $file) -Destination $appDestination
        }

        Write-Detail "Staged $($AppPayload.Count) app files -> bluetooth-app\$plat"
    }
}

# ----------------------------------------------------------------------------------------------
# Setup
# ----------------------------------------------------------------------------------------------

function Get-InstallerPath {
    param([Parameter(Mandatory)] [string] $BuildPlatform)
    Join-Path $SetupSolutionDir "main-bundle\bin\$BuildPlatform\$Configuration\$BundleName.exe"
}

function Invoke-SetupTarget {
    Write-Step 'Setup'

    foreach ($plat in $Platform) {
        Invoke-MSBuild -ProjectOrSolution $SetupSolution -BuildPlatform $plat `
            -Targets @('Restore', 'Rebuild') -SolutionDir $SetupSolutionDir

        $bundle = Get-InstallerPath -BuildPlatform $plat
        if (-not (Test-Path $bundle)) {
            throw "Installer bundle not found after build: $bundle"
        }
        Write-Detail "Built installer: $bundle"
    }
}

# ----------------------------------------------------------------------------------------------
# Release
# ----------------------------------------------------------------------------------------------

function Invoke-ReleaseTarget {
    param($Version)

    Write-Step 'Release'

    $folder = Join-Path $ReleaseRoot "bluetooth-$($Version.ReleaseLabel)"
    New-Item -ItemType Directory -Force -Path $folder | Out-Null

    foreach ($plat in $Platform) {
        $bundle = Get-InstallerPath -BuildPlatform $plat
        if (-not (Test-Path $bundle)) {
            Write-Note "Installer not found for $plat - run the Setup target: $bundle"
            continue
        }

        $name = "$InstallerName $($Version.SemVer)-$($plat.ToLowerInvariant()).exe"
        Copy-Item $bundle -Destination (Join-Path $folder $name) -Force
        Write-Detail "Installer -> $name"
    }

    Write-Host ''
    Write-Host "     Release folder: $folder" -ForegroundColor Green
}

# ----------------------------------------------------------------------------------------------
# Clean
# ----------------------------------------------------------------------------------------------

function Invoke-CleanTarget {
    Write-Step 'Clean'

    # The api staging folder is shared with build-plugins.ps1, so only the Bluetooth transport is
    # removed from it rather than the whole folder.
    foreach ($plat in @('x64', 'Arm64')) {
        foreach ($ext in @('dll', 'pdb')) {
            $path = Join-Path $ApiStagingRoot "$plat\$TransportBinary.$ext"
            if (Test-Path $path) {
                Remove-Item $path -Force
                Write-Detail "Removed $path"
            }
        }
    }

    if (Test-Path $AppStagingRoot) {
        Remove-Item $AppStagingRoot -Recurse -Force
        Write-Detail "Removed $AppStagingRoot"
    }
}

# ----------------------------------------------------------------------------------------------
# Main
# ----------------------------------------------------------------------------------------------

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

$targets = if ($Target -contains 'All') { @('Version', 'Service', 'App', 'Stage', 'Setup', 'Release') } else { $Target }

Write-Host ''
Write-Host 'Windows MIDI Services - Bluetooth MIDI preview build' -ForegroundColor White
Write-Detail "Repo          $RepoRoot"
Write-Detail "Targets       $($targets -join ', ')"
Write-Detail "Platforms     $($Platform -join ', ')"
Write-Detail "Configuration $Configuration"

if ($targets -contains 'Clean') {
    Invoke-CleanTarget
    if ($targets.Count -eq 1) { return }
}

$version = Get-BuildVersion

if ($targets -notcontains 'Version') {
    Write-Detail "Version       $($version.SemVer)"
}

if ($targets -contains 'Service' -or $targets -contains 'App' -or $targets -contains 'Setup') {
    $script:MSBuild = Resolve-MSBuild
    Write-Detail "MSBuild       $script:MSBuild"
}

if ($targets -contains 'Version') { Invoke-VersionTarget $version }
if ($targets -contains 'Service') { Invoke-ServiceTarget }
if ($targets -contains 'App') { Invoke-AppTarget }
if ($targets -contains 'Stage') { Invoke-StageTarget }
if ($targets -contains 'Setup') { Invoke-SetupTarget }
if ($targets -contains 'Release') { Invoke-ReleaseTarget $version }

$stopwatch.Stop()
Write-Host ''
Write-Host ("Done in {0:hh\:mm\:ss}" -f $stopwatch.Elapsed) -ForegroundColor Green
Write-Host ''

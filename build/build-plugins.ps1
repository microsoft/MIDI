<#
.SYNOPSIS
    Builds, stages and packages the Windows MIDI Services service plugin installers.

.DESCRIPTION
    Replaces the old Nuke build (build/nuke_build-plugins). Produces standalone installers for
    the Network MIDI 2.0 and Basic Loopback MIDI service transports, for x64 and Arm64.

    This is a SEPARATE release train from the App SDK (build/build-sdk.ps1). It owns
    build/staging/version/BundleInfo.wxi; the App SDK owns build/staging/version/AppSdkVersion.wxi.
    All version numbers come from build/version-plugins.json.

.PARAMETER Target
    One or more of:
      Version  Compute versions and write BundleInfo.wxi.
      Service  Build src/in-box/Midi2.sln for each platform.
      Stage    Copy the transport binaries into build/staging/api, and the shared API headers
               into src/shared/api-ref (mididiag and midi2monitor have that on their IncludePath).
      Setup    Build the Network MIDI 2.0 and Basic Loopback installers.
      Release  Collect the installers into build/release/plugins-<version>.
      Clean    Delete plugin staging and service solution output folders.
      All      Version, Service, Stage, Setup, Release.

.PARAMETER BuildNumber
    Overrides the 'build' field in version-plugins.json without modifying the file. For CI.

.PARAMETER BumpBuildNumber
    Increments and persists the 'build' field in version-plugins.json before computing versions.

.EXAMPLE
    .\build-plugins.ps1
    Full plugin release build for x64 and Arm64.

.EXAMPLE
    .\build-plugins.ps1 -Target Setup -Platform x64
    Rebuild just the installers for x64 against whatever is already staged.
#>
[CmdletBinding()]
param(
    # Comma-separated is accepted as a single token so this works through `pwsh -File`
    # and build-plugins.cmd, which do not split array arguments.
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

$Target = Expand-Argument -Value $Target -Allowed @('All', 'Version', 'Service', 'Stage', 'Setup', 'Release', 'Clean') -Name 'Target'
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

# Only built when a selected plugin ships a GUI app alongside its transport.
$AppSdkSolution = Join-Path $ApiRoot 'Midi2-AppSDK.sln'
$AppSdkOutRoot = Join-Path $ApiRoot 'vsfiles-sdk\out'

$StagingRoot = Join-Path $BuildRoot 'staging'
$ReleaseRoot = Join-Path $BuildRoot 'release'
$ApiStagingRoot = Join-Path $StagingRoot 'api'
$VersionStagingFolder = Join-Path $StagingRoot 'version'

# Owned by this build. The App SDK writes AppSdkVersion.wxi instead, so the two trains cannot
# overwrite each other's version.
$BundleInfoFile = Join-Path $VersionStagingFolder 'BundleInfo.wxi'

$VersionFile = Join-Path $BuildRoot 'version-plugins.json'

$ApiReferenceRoot = Join-Path $SourceRoot 'shared\api-ref'
$ApiIncludeFolder = Join-Path $ApiRoot 'Inc'
$NetworkTransportFolder = Join-Path $ApiRoot 'Transport\UdpNetworkMidi2Transport'

# The .wxs files resolve staging as "$(env.MIDI_REPO_ROOT)\build\staging", so this must NOT
# have a trailing separator.
$env:MIDI_REPO_ROOT = $RepoRoot.TrimEnd('\')

# Each plugin: the transport binaries it stages, its installer solution, and its bundle name.
# AppName, when present, is a GUI tool that ships in the same installer as the transport, because
# the tool is useless without it. AppFolder must match ToolLauncher.cpp in midi-settings in the
# Settings app, which resolves tools by convention path.
$Plugins = @(
    [pscustomobject]@{
        Name         = 'Network MIDI 2.0'
        Binary       = 'Midi2.NetworkMidiTransport'
        SolutionDir  = Join-Path $SourceRoot 'installers\oob-setup-network'
        Solution     = 'midi-services-network-midi-preview-setup.sln'
        BundleName   = 'WindowsMidiServicesNetworkMidiSetup'
        InstallerName = 'Windows MIDI Services (Network MIDI 2.0 Preview)'
        AppName      = 'midinetworksetup'
        AppStagingName = 'network-app'
    }
    [pscustomobject]@{
        Name         = 'Basic Loopback MIDI'
        Binary       = 'Midi2.BasicLoopbackMidiTransport'
        SolutionDir  = Join-Path $SourceRoot 'installers\oob-setup-basic-loopback'
        Solution     = 'midi-services-basic-loopback-setup.sln'
        BundleName   = 'WindowsMidiServicesBasicLoopbackSetup'
        InstallerName = 'Windows MIDI Services (Basic MIDI 1.0 Loopback Preview)'
        AppName      = $null
        AppStagingName = $null
    }
)

# The app payload, as referenced by each plugin's WindowsMidiServices.wxs. The .exp, .lib and
# .pdb files in the same output folder are deliberately not shipped; the pdbs alone are huge.
# Names are prefixed with the app name where they carry it.
$AppPayloadCommon = @(
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

# Headers consumers compile against. mididiag and midi2monitor put src/shared/api-ref/<platform>
# on their IncludePath, so the App SDK build depends on this being populated.
$ApiReferenceHeaders = @(
    (Join-Path $ApiIncludeFolder 'hstring_util.h')
    (Join-Path $ApiIncludeFolder 'wstring_util.h')
    (Join-Path $ApiIncludeFolder 'json_defs.h')
    (Join-Path $ApiIncludeFolder 'json_helpers.h')
    (Join-Path $ApiIncludeFolder 'loopback_ids.h')
    (Join-Path $NetworkTransportFolder 'network_json_defs.h')
)

$ApiReferencePlatforms = @('x64', 'Arm64', 'Arm64EC')

# ----------------------------------------------------------------------------------------------
# Output helpers
# ----------------------------------------------------------------------------------------------

$script:StepNumber = 0

function Write-Step {
    param([string] $Message)
    $script:StepNumber++
    Write-Host ''
    Write-Host ('=' * 96) -ForegroundColor DarkCyan
    Write-Host (' {0,2}. {1}' -f $script:StepNumber, $Message) -ForegroundColor Cyan
    Write-Host ('=' * 96) -ForegroundColor DarkCyan
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

    # Same scheme as the App SDK: no build number in the SemVer string; 'patch' distinguishes
    # releases. Stable drops the prerelease tag entirely.
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

    # A plugin that ships a GUI app needs the App SDK solution as well, which is the only
    # reason this script ever touches that train.
    if (@($Plugins | Where-Object { $_.AppName }).Count -gt 0) {
        Write-Step 'Build plugin apps'

        foreach ($plat in $Platform) {
            Invoke-MSBuild -ProjectOrSolution $AppSdkSolution -BuildPlatform $plat `
                -Properties @{ 'NoWarn' = 'MIDL2111' }
        }
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
        $sourceFolder = Join-Path $ServiceOutRoot "$plat\$Configuration"
        $destination = Join-Path $ApiStagingRoot $plat

        foreach ($plugin in $Plugins) {
            foreach ($ext in @('dll', 'pdb')) {
                Copy-Staged -Source (Join-Path $sourceFolder "$($plugin.Binary).$ext") -Destination $destination
            }
        }

        Write-Detail "Staged $($Plugins.Count) transports -> api\$plat"

        foreach ($plugin in $Plugins | Where-Object { $_.AppName }) {
            $appSource = Join-Path $AppSdkOutRoot "$($plugin.AppName)\$plat\$Configuration"
            $appDestination = Join-Path $StagingRoot "$($plugin.AppStagingName)\$plat"

            $payload = @("$($plugin.AppName).exe", "$($plugin.AppName).winmd") + $AppPayloadCommon

            foreach ($file in $payload) {
                Copy-Staged -Source (Join-Path $appSource $file) -Destination $appDestination
            }

            Write-Detail "Staged $($payload.Count) app files -> $($plugin.AppStagingName)\$plat"
        }
    }

    # Not needed by the installers, but mididiag and midi2monitor compile against these, so the
    # App SDK build breaks on a clean clone if this is skipped.
    foreach ($plat in $ApiReferencePlatforms) {
        $destination = Join-Path $ApiReferenceRoot $plat
        foreach ($header in $ApiReferenceHeaders) {
            Copy-Staged -Source $header -Destination $destination
        }
    }

    Write-Detail "Refreshed shared API headers for $($ApiReferencePlatforms -join ', ')"
}

# ----------------------------------------------------------------------------------------------
# Setup
# ----------------------------------------------------------------------------------------------

function Get-InstallerPath {
    param([Parameter(Mandatory)] $Plugin, [Parameter(Mandatory)] [string] $BuildPlatform)
    Join-Path $Plugin.SolutionDir "main-bundle\bin\$BuildPlatform\$Configuration\$($Plugin.BundleName).exe"
}

function Invoke-SetupTarget {
    Write-Step 'Setup'

    foreach ($plugin in $Plugins) {
        foreach ($plat in $Platform) {
            $solution = Join-Path $plugin.SolutionDir $plugin.Solution

            Invoke-MSBuild -ProjectOrSolution $solution -BuildPlatform $plat `
                -Targets @('Restore', 'Rebuild') -SolutionDir $plugin.SolutionDir

            $bundle = Get-InstallerPath -Plugin $plugin -BuildPlatform $plat
            if (-not (Test-Path $bundle)) {
                throw "Installer bundle not found after build: $bundle"
            }
            Write-Detail "Built installer: $bundle"
        }
    }
}

# ----------------------------------------------------------------------------------------------
# Release
# ----------------------------------------------------------------------------------------------

function Invoke-ReleaseTarget {
    param($Version)

    Write-Step 'Release'

    $folder = Join-Path $ReleaseRoot "plugins-$($Version.ReleaseLabel)"
    New-Item -ItemType Directory -Force -Path $folder | Out-Null

    foreach ($plugin in $Plugins) {
        foreach ($plat in $Platform) {
            $bundle = Get-InstallerPath -Plugin $plugin -BuildPlatform $plat
            if (-not (Test-Path $bundle)) {
                Write-Note "Installer not found for $($plugin.Name) $plat - run the Setup target: $bundle"
                continue
            }

            $name = "$($plugin.InstallerName) $($Version.SemVer)-$($plat.ToLowerInvariant()).exe"
            Copy-Item $bundle -Destination (Join-Path $folder $name) -Force
            Write-Detail "Installer -> $name"
        }
    }

    Write-Host ''
    Write-Host "     Release folder: $folder" -ForegroundColor Green
}

# ----------------------------------------------------------------------------------------------
# Clean
# ----------------------------------------------------------------------------------------------

function Invoke-CleanTarget {
    Write-Step 'Clean'

    $paths = @($ApiStagingRoot, (Join-Path $ServiceOutRoot 'intermediate'))

    foreach ($plugin in $Plugins | Where-Object { $_.AppStagingName }) {
        $paths += Join-Path $StagingRoot $plugin.AppStagingName
    }

    foreach ($path in $paths) {
        if (Test-Path $path) {
            Remove-Item $path -Recurse -Force
            Write-Detail "Removed $path"
        }
    }
}

# ----------------------------------------------------------------------------------------------
# Main
# ----------------------------------------------------------------------------------------------

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

$targets = if ($Target -contains 'All') { @('Version', 'Service', 'Stage', 'Setup', 'Release') } else { $Target }

Write-Host ''
Write-Host 'Windows MIDI Services - service plugins build' -ForegroundColor White
Write-Detail "Repo          $RepoRoot"
Write-Detail "Targets       $($targets -join ', ')"
Write-Detail "Platforms     $($Platform -join ', ')"
Write-Detail "Configuration $Configuration"
Write-Detail "Plugins       $(($Plugins | ForEach-Object { $_.Name }) -join ', ')"

if ($targets -contains 'Clean') {
    Invoke-CleanTarget
    if ($targets.Count -eq 1) { return }
}

$version = Get-BuildVersion

if ($targets -notcontains 'Version') {
    Write-Detail "Version       $($version.SemVer)"
}

if ($targets -contains 'Service' -or $targets -contains 'Setup') {
    $script:MSBuild = Resolve-MSBuild
    Write-Detail "MSBuild       $script:MSBuild"
}

if ($targets -contains 'Version') { Invoke-VersionTarget $version }
if ($targets -contains 'Service') { Invoke-ServiceTarget }
if ($targets -contains 'Stage') { Invoke-StageTarget }
if ($targets -contains 'Setup') { Invoke-SetupTarget }
if ($targets -contains 'Release') { Invoke-ReleaseTarget $version }

$stopwatch.Stop()
Write-Host ''
Write-Host ("Done in {0:hh\:mm\:ss}" -f $stopwatch.Elapsed) -ForegroundColor Green
Write-Host ''

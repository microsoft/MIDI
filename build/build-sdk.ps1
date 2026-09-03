<#
.SYNOPSIS
    Builds, stages, packages and installs Windows MIDI Services App SDK, tools and setup.

.DESCRIPTION
    Replaces the old Nuke build (build/nuke_build). Everything the release needs now lives in
    a single solution (src/in-box/Midi2-AppSDK.sln), so this script is mostly: build that solution
    per platform, publish the .NET apps, copy the results into build/staging, then build the
    WiX installer against build/staging.

    All version numbers come from build/version.json. Nothing else in the repo should carry a
    hand-maintained version.

.PARAMETER Target
    One or more of:
      Version  Compute versions; write BundleInfo.wxi and stamp the NuGet nuspec.
      Sdk      Build Midi2-AppSDK.sln (Arm64, Arm64EC for Arm64X, x64) and the .NET apps.
      Stage    Publish/copy all binaries and assets into build/staging.
      Setup    Generate the WiX file lists and build the installer bundle.
      Release  Collect the nupkg and installers into build/release/<version>.
      Clean    Delete staging, release and solution output folders.
      All      Version, Sdk, Stage, Setup, Release.

.PARAMETER BuildNumber
    Overrides the 'build' field in version.json without modifying the file. Intended for CI
    (for example -BuildNumber $env:GITHUB_RUN_NUMBER).

.PARAMETER BumpBuildNumber
    Increments and persists the 'build' field in version.json before computing versions.

.EXAMPLE
    .\build-sdk.ps1
    Full release build for x64 and Arm64.

.EXAMPLE
    .\build-sdk.ps1 -Target Version
    Just recompute and write version files. Fast; useful for checking what a build would produce.

.EXAMPLE
    .\build-sdk.ps1 -Target Sdk,Stage -Platform x64
    Build and stage x64 only, skipping the installer.
#>
[CmdletBinding()]
param(
    # Comma-separated is accepted as a single token so this works through `pwsh -File`
    # and build.cmd, which do not split array arguments.
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

$Target = Expand-Argument -Value $Target -Allowed @('All', 'Version', 'Sdk', 'Stage', 'Setup', 'Release', 'Clean') -Name 'Target'
$Platform = Expand-Argument -Value $Platform -Allowed @('x64', 'Arm64') -Name 'Platform'

# ----------------------------------------------------------------------------------------------
# Paths
# ----------------------------------------------------------------------------------------------

$BuildRoot = $PSScriptRoot
$RepoRoot = Split-Path -Parent $BuildRoot

$SourceRoot = Join-Path $RepoRoot 'src'
$ApiRoot = Join-Path $SourceRoot 'in-box'
$UserToolsRoot = Join-Path $ApiRoot 'user-tools'

$SdkSolution = Join-Path $ApiRoot 'Midi2-AppSDK.sln'
$SdkOutRoot = Join-Path $ApiRoot 'vsfiles-sdk\out'
$SdkIntermediateRoot = Join-Path $ApiRoot 'vsfiles-sdk\intermediate'
$SdkNuGetOutput = Join-Path $ApiRoot 'vsfiles-sdk\PublishedNuGet'

$NuspecFile = Join-Path $ApiRoot 'Client\WinRT\NuGet\Windows.Devices.Midi2.NuGet\nuget\Windows.Devices.Midi2.nuspec'

$ConsoleProject = Join-Path $UserToolsRoot 'midi-console\Midi\Midi.csproj'
$PowerShellProject = Join-Path $ApiRoot 'Client\WinRT\powershell\WindowsMidiServices.csproj'

$SetupSolutionRoot = Join-Path $SourceRoot 'installers\api-and-tools-installer'
$SetupSolution = Join-Path $SetupSolutionRoot 'midi-services-app-sdk-runtime-setup.sln'

$DesignRoot = Join-Path $RepoRoot 'design'
$CollectMidiLogsRoot = Join-Path $ApiRoot 'CollectMidiLogs'

$StagingRoot = Join-Path $BuildRoot 'staging'
$ReleaseRoot = Join-Path $BuildRoot 'release'
$VersionStagingFolder = Join-Path $StagingRoot 'version'

# Deliberately NOT BundleInfo.wxi: that file belongs to the service/plugins installers, which are
# a separate release train on a separate version (see build\build-plugins.ps1). Sharing it means
# whichever build ran last wins.
$AppSdkVersionFile = Join-Path $VersionStagingFolder 'AppSdkVersion.wxi'

$VersionFile = Join-Path $BuildRoot 'version.json'

# The .wxs files resolve staging as "$(env.MIDI_REPO_ROOT)\build\staging", so this must NOT
# have a trailing separator.
$env:MIDI_REPO_ROOT = $RepoRoot.TrimEnd('\')

# In-box tools that ship in the SDK runtime installer. Name = project name = exe name.
# Console tools install flat into "Program Files\Windows MIDI Services\Tools".
$ConsoleTools = @(
    'mididiag'
    'midiksinfo'
    'midimdnsinfo'
    'midi1monitor'
    'midi1enum'
    'midiapimode'
    'midifixreg'
)

# GUI tools each install into their OWN subfolder of Tools. MIDI Settings resolves the others by
# convention at %ProgramFiles%\Windows MIDI Services\Tools\<Folder>\<exe> - see ToolLauncher.cpp
# in midi-settings - so Folder must match that table exactly.
# Display names come from Resources.resw (ToolApp_*_Name) so the Start Menu matches the app.
# Network MIDI 2.0 Setup and Bluetooth MIDI Setup are deliberately NOT here: each ships in the
# installer that carries its transport, because the app is useless without it, and two installers
# writing the same files to the same folder would break each other's uninstall.
$GuiTools = @(
    [pscustomobject]@{ Name = 'midisettings';      Folder = 'Settings';     Display = 'MIDI Settings';          DirectoryId = 'TOOL_SETTINGS_FOLDER' }
    [pscustomobject]@{ Name = 'midiloopbacksetup'; Folder = 'LoopSetup';    Display = 'MIDI Loopback Setup';    DirectoryId = 'TOOL_LOOPSETUP_FOLDER' }
    [pscustomobject]@{ Name = 'midiscratchpad';    Folder = 'ScratchPad';   Display = 'MIDI Scratch Pad';       DirectoryId = 'TOOL_SCRATCHPAD_FOLDER' }
    [pscustomobject]@{ Name = 'midikeyboard';      Folder = 'Keyboard';     Display = 'Windows MIDI Keyboard';  DirectoryId = 'TOOL_KEYBOARD_FOLDER' }
    [pscustomobject]@{ Name = 'midisysextool';     Folder = 'SysEx';        Display = 'MIDI SysEx Tool';        DirectoryId = 'TOOL_SYSEX_FOLDER' }
    [pscustomobject]@{ Name = 'midi2monitor';      Folder = 'Monitor';      Display = 'MIDI Monitor';           DirectoryId = 'TOOL_MONITOR_FOLDER' }
    [pscustomobject]@{ Name = 'miditroubleshooter'; Folder = 'Troubleshooter'; Display = 'MIDI Troubleshooting and Repair'; DirectoryId = 'TOOL_TROUBLESHOOTER_FOLDER' }
)

# Start Menu group shared by every MIDI GUI app, including MIDI Settings.
$StartMenuFolderName = 'Windows MIDI (Preview)'

# Linker/metadata leftovers in a native project's output folder. Not shipped.
$BuildOnlyExtensions = @('.exp', '.lib', '.winmd', '.ipdb', '.iobj', '.pdb')

# Surfaced in the generated version headers and shown in midi2monitor's settings dialog.
$SdkBuildSource = 'GitHub Preview'

# Arm64EC is built only to produce the Arm64X SDK binary. These are Windows.Devices.Midi2 and the
# projects it declares as solution dependencies, in build order - static libs are not rebuilt by
# their dependents, so they have to come first.
$Arm64EcProjects = @(
    'Libs\SDK-MidiPluginConfigurationLib\MidiPluginConfigurationLib.vcxproj'
    'Libs\SDK-MidiEndpointNamingLib\MidiEndpointNamingLib.vcxproj'
    'Libs\SDK-MidiPnpLib\MidiPnpLib.vcxproj'
    'Client\WinRT\com-extensions-idl\com-extensions-idl.vcxproj'
    'Client\WinRT\core\Windows.Devices.Midi2.vcxproj'
)

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
        # Preserve the comment block and key order by round-tripping the whole object.
        $json | ConvertTo-Json -Depth 8 | Set-Content $VersionFile -Encoding UTF8
        Write-Detail "Bumped build number to $($json.build) in version.json"
    }

    $effectiveBuild = if ($BuildNumber -ge 0) { $BuildNumber } else { [int]$json.build }

    $majorMinorPatch = '{0}.{1}.{2}' -f $json.major, $json.minor, $json.patch

    # SemVer 2 for NuGet, e.g. 0.99.57-devpreview.5. The build number is deliberately absent:
    # 'patch' is what distinguishes releases. Stable drops the prerelease tag entirely.
    if ($json.channel -eq 'stable') {
        $semVer = $majorMinorPatch
    }
    else {
        $semVer = '{0}-{1}.{2}' -f $majorMinorPatch, $json.channel, $json.channelNumber
    }

    # Four-part numeric, for assemblies, file versions, MSI and the bundle.
    $numericVersion = '{0}.{1}' -f $majorMinorPatch, $effectiveBuild

    [pscustomobject]@{
        Major           = [int]$json.major
        Minor           = [int]$json.minor
        Patch           = [int]$json.patch
        Build           = $effectiveBuild
        Channel         = [string]$json.channel
        ChannelNumber   = [int]$json.channelNumber
        VersionName     = [string]$json.versionName
        NuGetPackageId  = [string]$json.nuGetPackageId
        MajorMinorPatch = $majorMinorPatch
        SemVer          = $semVer
        NumericVersion  = $numericVersion
        # Folder-safe label for the release folder.
        ReleaseLabel    = ($semVer -replace '[^\w\.\-]', '-')
    }
}

function Invoke-VersionTarget {
    param($Version)

    Write-Step 'Version'

    Write-Detail "Name            $($Version.VersionName)"
    Write-Detail "NuGet / SemVer  $($Version.SemVer)"
    Write-Detail "Numeric / MSI   $($Version.NumericVersion)"

    New-Item -ItemType Directory -Force -Path $VersionStagingFolder | Out-Null

    # --- AppSdkVersion.wxi, consumed by every .wxs in the App SDK installer ------------------
    $bundleInfo = @"
<?xml version="1.0" encoding="utf-8"?>
<!-- Generated by build\build-sdk.ps1 from build\version.json. Do not edit. -->
<Include>
  <?define SetupVersionName="$($Version.VersionName)" ?>
  <?define SetupVersionNumber="$($Version.NumericVersion)" ?>
  <?define MidiSdkAndToolsVersion="$($Version.NumericVersion)" ?>
  <?define MidiSdkAndToolsSemVer="$($Version.SemVer)" ?>
  <?define StartMenuFolderName="$StartMenuFolderName" ?>
</Include>
"@
    Set-Content -Path $AppSdkVersionFile -Value $bundleInfo -Encoding UTF8
    Write-Detail "Wrote $AppSdkVersionFile"

    # --- Version headers, replacing the retired T4 generator ---------------------------------
    # midi2monitor's settings dialog shows *_BUILD_VERSION_FULL / *_BUILD_SOURCE; mididiag and
    # the t\ SDK split projects include the headers too. Generated here so they finally carry the
    # SDK version rather than whatever the plugins build last wrote.
    $isPreview = if ($Version.Channel -eq 'stable') { 'false' } else { 'true' }
    $previewTag = if ($Version.Channel -eq 'stable') { '' } else { "$($Version.Channel).$($Version.ChannelNumber)" }

    foreach ($header in @(
            @{ File = 'WindowsMidiServicesVersion.h'; Prefix = 'WINDOWS_MIDI_SERVICES_NUGET' }
            @{ File = 'WindowsMidiServicesSdkRuntimeVersion.h'; Prefix = 'WINDOWS_MIDI_SERVICES_SDK_RUNTIME' }
        )) {

        $p = $header.Prefix
        $content = @"
// This file is generated by build\build-sdk.ps1 from build\version.json. Do not edit.
// The version information here represents the Windows MIDI Services App SDK version
// this binary was built against.

#ifndef $($p)_VERSION_INCLUDE
#define $($p)_VERSION_INCLUDE

#define $($p)_BUILD_IS_PREVIEW                         $isPreview
#define $($p)_BUILD_SOURCE                             L"$SdkBuildSource"
#define $($p)_BUILD_DATE                               L"$(Get-Date -Format 'yyyy-MM-dd')"
#define $($p)_BUILD_VERSION_NAME                       L"$($Version.VersionName)"
#define $($p)_BUILD_VERSION_FULL                       L"$($Version.SemVer)"
#define $($p)_BUILD_VERSION_MAJOR                      $($Version.Major)
#define $($p)_BUILD_VERSION_MINOR                      $($Version.Minor)
#define $($p)_BUILD_VERSION_PATCH                      $($Version.Patch)
#define $($p)_BUILD_VERSION_BUILD_NUMBER               $($Version.Build)
#define $($p)_BUILD_PREVIEW                            L"$previewTag"
#define $($p)_BUILD_VERSION_FILE                       L"$($Version.NumericVersion)"

#endif
"@
        Set-Content -Path (Join-Path $VersionStagingFolder $header.File) -Value $content -Encoding UTF8
    }

    Write-Detail 'Wrote version headers (WindowsMidiServicesVersion.h, WindowsMidiServicesSdkRuntimeVersion.h)'

    # --- Stamp the nuspec version ------------------------------------------------------------
    # The nuspec is checked in with a literal <version>, so rewrite just that element.
    $nuspec = Get-Content $NuspecFile -Raw
    $updated = [regex]::Replace($nuspec, '<version>[^<]*</version>', "<version>$($Version.SemVer)</version>", 1)

    if ($updated -ne $nuspec) {
        Set-Content -Path $NuspecFile -Value $updated -Encoding UTF8 -NoNewline
        Write-Detail "Stamped nuspec version -> $($Version.SemVer)"
    }
    else {
        Write-Detail 'Nuspec version already current'
    }

    # Windows Installer only compares major.minor.build (our patch). Warn when a rebuild would
    # not upgrade over the previously installed package.
    Write-Note "MSI upgrade detection uses only $($Version.MajorMinorPatch) - bump 'patch' in version.json for a release that must replace an installed one."
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

    # 64-bit MSBuild. The Arm64EC/Arm64X link steps need the amd64 host tools.
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

    # The app projects reach Windows.Devices.Midi2.vcxproj through more than one global-property
    # set, so MSBuild builds it twice in a single invocation. Both writes target the same
    # OutDir/IntDir, and in parallel they collide on the FileTracker logs (FTK1011).
    $msbuildArgs += if ($Serial) { '/m:1' } else { '/m' }

    if ($Targets.Count -gt 0) { $msbuildArgs += "/t:$($Targets -join ';')" }

    # Several vcxproj files include headers relative to $(SolutionDir); building a project
    # directly without it fails with C1083 on MidiDefs.h.
    if ($SolutionDir) { $msbuildArgs += "/p:SolutionDir=$($SolutionDir.TrimEnd('\'))\" }

    foreach ($key in $Properties.Keys) { $msbuildArgs += "/p:$key=$($Properties[$key])" }

    Write-Detail "msbuild $(Split-Path -Leaf $ProjectOrSolution) [$Configuration|$BuildPlatform]"

    & $script:MSBuild @msbuildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed ($LASTEXITCODE): $ProjectOrSolution [$Configuration|$BuildPlatform]"
    }
}

function Get-VersionProperties {
    param($Version)
    @{
        'Version'             = $Version.SemVer
        'VersionPrefix'       = $Version.MajorMinorPatch
        'AssemblyVersion'     = $Version.NumericVersion
        'FileVersion'         = $Version.NumericVersion
        'InformationalVersion' = $Version.SemVer
    }
}

# ----------------------------------------------------------------------------------------------
# Build
# ----------------------------------------------------------------------------------------------

function Invoke-SdkTarget {
    param($Version)

    Write-Step 'Build SDK, tools and apps'

    $versionProps = Get-VersionProperties $Version
    # winmd/dll platform mismatch is expected for Arm64EC.
    $versionProps['NoWarn'] = 'MSB3271'

    $buildArm = $Platform -contains 'Arm64'

    # Order matters.
    #
    #   1. Arm64      - produces the classic Arm64 binaries.
    #   2. Arm64EC    - Windows.Devices.Midi2.vcxproj sets <BuildAsX>true</BuildAsX> on its
    #                   Arm64EC configurations, so this pass emits the Arm64X binary into the
    #                   arm64EC output folder. That is what the nuspec packages as win-arm64.
    #   3. x64        - last, because the NuGet project has GeneratePackageOnBuild and its
    #                   nuspec pulls from BOTH the x64 and arm64EC output folders.
    if ($buildArm) {
        # The NuGet project packs on build and its nuspec pulls from the x64 AND arm64EC output
        # folders, so suppress packing until the final x64 pass or a clean machine fails here.
        $armProps = $versionProps.Clone()
        $armProps['GeneratePackageOnBuild'] = 'false'

        Invoke-MSBuild -ProjectOrSolution $SdkSolution -BuildPlatform 'Arm64' -Properties $armProps

        # Arm64EC exists only to produce the Arm64X SDK binary, so build just that project and
        # its dependencies rather than dragging the whole solution through a third pass.
        # Solution-level target names (/t:Windows_Devices_Midi2) do NOT work here - MSBuild
        # forwards the name to every project and they all fail with MSB4057.
        foreach ($project in $Arm64EcProjects) {
            Invoke-MSBuild -ProjectOrSolution (Join-Path $ApiRoot $project) -BuildPlatform 'Arm64EC' `
                -Properties $armProps -SolutionDir $ApiRoot
        }
    }
    else {
        Write-Note 'Arm64 not requested - the NuGet package will not contain Arm64X binaries.'
    }

    if ($Platform -contains 'x64') {
        Invoke-MSBuild -ProjectOrSolution $SdkSolution -BuildPlatform 'x64' -Properties $versionProps
    }

    if (Test-Path $SdkNuGetOutput) {
        $pkg = Join-Path $SdkNuGetOutput "$($Version.NuGetPackageId).$($Version.SemVer).nupkg"
        if (Test-Path $pkg) {
            Write-Detail "NuGet package: $pkg"
        }
        else {
            Write-Note "Expected NuGet package was not produced: $pkg"
        }
    }
}

# ----------------------------------------------------------------------------------------------
# Staging
# ----------------------------------------------------------------------------------------------

function Copy-Staged {
    param(
        [Parameter(Mandatory)] [string] $Source,
        [Parameter(Mandatory)] [string] $Destination,
        [switch] $Optional
    )

    if (-not (Test-Path $Source)) {
        if ($Optional) { return }
        throw "Staging source not found: $Source"
    }

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Copy-Item -Path $Source -Destination $Destination -Force
}

# Symbols are archived with the release rather than installed. A customer is almost never in a
# position to debug in place, and the pdbs dwarf the binaries they belong to, but we still need
# the exact ones from a given build to make sense of a crash reported against it later.
function Move-StagedSymbols {
    param(
        [Parameter(Mandatory)] [string] $Folder,
        [Parameter(Mandatory)] [string] $BuildPlatform
    )

    if (-not (Test-Path $Folder)) { return }

    $symbols = @(Get-ChildItem $Folder -File -Recurse -Filter '*.pdb')
    if ($symbols.Count -eq 0) { return }

    $symbolStaging = Join-Path $StagingRoot "symbols\$BuildPlatform"
    New-Item -ItemType Directory -Force -Path $symbolStaging | Out-Null

    foreach ($pdb in $symbols) {
        Move-Item $pdb.FullName -Destination (Join-Path $symbolStaging $pdb.Name) -Force
    }

    Write-Detail "  $($symbols.Count) pdb -> staging\symbols\$BuildPlatform"
}

function Publish-DotNetApp {
    param(
        [Parameter(Mandatory)] [string] $Project,
        [Parameter(Mandatory)] [string] $BuildPlatform,
        [Parameter(Mandatory)] [string] $RuntimeIdentifier,
        [Parameter(Mandatory)] [string] $Destination,
        [Parameter(Mandatory)] $Version
    )

    if (Test-Path $Destination) { Remove-Item $Destination -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null

    # These csproj files ProjectReference the C++ SDK, so `dotnet publish` cannot even evaluate
    # them - the dotnet CLI has no VCTargetsPath and fails with MSB4278. Full MSBuild only.
    # SolutionDir is mandatory: the referenced vcxproj builds its OutDir/IntDir from
    # $(SolutionDir), and without it MSBuild creates a second output tree under the vcxproj's
    # own folder and collides on tlog files.
    # PublishDir uses forward slashes: a trailing backslash before a closing quote gets eaten by
    # the Windows command-line parser when the path contains spaces.
    $publishDir = $Destination.Replace('\', '/') + '/'

    Write-Detail "publish $(Split-Path -Leaf $Project) [$BuildPlatform / $RuntimeIdentifier]"

    Invoke-MSBuild -ProjectOrSolution $Project -BuildPlatform $BuildPlatform `
        -Targets @('Restore', 'Publish') `
        -SolutionDir $ApiRoot `
        -Serial `
        -Properties @{
            'RuntimeIdentifier'    = $RuntimeIdentifier
            'SelfContained'        = 'false'
            'PublishDir'           = $publishDir
            'PublishSingleFile'    = 'false'
            'PublishTrimmed'       = 'false'
            'PublishReadyToRun'    = 'false'
            'PublishProtocol'      = 'FileSystem'
            'Version'              = $Version.SemVer
            'VersionPrefix'        = $Version.MajorMinorPatch
            'AssemblyVersion'      = $Version.NumericVersion
            'FileVersion'          = $Version.NumericVersion
        }

    $produced = @(Get-ChildItem $Destination -File -ErrorAction SilentlyContinue)
    if ($produced.Count -eq 0) {
        throw "Publish reported success but produced no files in $Destination"
    }

    # @() matters: StrictMode turns $null.Count into an error when there are no subfolders.
    $subFolders = @(Get-ChildItem $Destination -Directory -ErrorAction SilentlyContinue)
    Write-Detail "  $($produced.Count) files, $($subFolders.Count) subfolders"
}

function Get-MSBuildProperty {
    param(
        [Parameter(Mandatory)] [string] $Project,
        [Parameter(Mandatory)] [string] $BuildPlatform,
        [Parameter(Mandatory)] [string] $Name,
        [hashtable] $Properties = @{}
    )

    $msbuildArgs = @(
        $Project
        "-getProperty:$Name"
        "/p:Configuration=$Configuration"
        "/p:Platform=$BuildPlatform"
        "/p:SolutionDir=$($ApiRoot.TrimEnd('\'))\"
        '/nologo'
        '/nr:false'
    )
    foreach ($key in $Properties.Keys) { $msbuildArgs += "/p:$key=$($Properties[$key])" }

    $value = @(& $script:MSBuild @msbuildArgs) | Where-Object { $_ } | Select-Object -Last 1
    if ($LASTEXITCODE -ne 0 -or -not $value) {
        throw "Could not evaluate $Name for $Project [$BuildPlatform]"
    }

    return $value.Trim()
}

function Invoke-StageTarget {
    param($Version)

    Write-Step 'Stage'

    foreach ($plat in $Platform) {

        $rid = if ($plat -eq 'Arm64') { 'win-arm64' } else { 'win-x64' }

        # --- SDK runtime and in-box tools ---------------------------------------------------
        # Arm64X implementation binaries land in the arm64EC output folder, so that is where
        # the Arm64 staging copy has to come from.
        $sdkBinarySourcePlatform = if ($plat -eq 'Arm64') { 'Arm64EC' } else { $plat }

        $appSdkStaging = Join-Path $StagingRoot "app-sdk\$plat"
        if (Test-Path $appSdkStaging) { Remove-Item $appSdkStaging -Recurse -Force }
        New-Item -ItemType Directory -Force -Path $appSdkStaging | Out-Null

        $sdkBinFolder = Join-Path $SdkOutRoot "Windows.Devices.Midi2\$sdkBinarySourcePlatform\$Configuration"

        # No .winmd and no .pdb. Metadata is read at compile time only, and the NuGet package is
        # where a developer gets it. Symbols are archived with the release instead.
        foreach ($ext in @('dll', 'pri')) {
            Copy-Staged -Source (Join-Path $sdkBinFolder "Windows.Devices.Midi2.$ext") -Destination $appSdkStaging
        }

        # Console tools sit flat in Tools\, next to the SDK copy above.
        foreach ($tool in $ConsoleTools) {
            Copy-Staged -Source (Join-Path $SdkOutRoot "$tool\$plat\$Configuration\$tool.exe") -Destination $appSdkStaging
        }

        # Each GUI tool gets its own folder. These are unpackaged WinUI apps, so the whole build
        # output has to ship: .xbf files (including subfolders like Styles\, which must keep
        # their structure), the app's own resources.pri, and the WindowsAppRuntime bootstrapper.
        # Without those the app will not start at all.
        foreach ($tool in $GuiTools) {
            $toolStaging = Join-Path $appSdkStaging $tool.Folder
            $toolOutput = Join-Path $SdkOutRoot "$($tool.Name)\$plat\$Configuration"

            if (-not (Test-Path $toolOutput)) { throw "GUI tool output not found: $toolOutput" }

            New-Item -ItemType Directory -Force -Path $toolStaging | Out-Null
            Copy-Item -Path (Join-Path $toolOutput '*') -Destination $toolStaging -Recurse -Force

            # Build-time only; shipping these would bloat the installer for no benefit.
            Get-ChildItem $toolStaging -Recurse -File |
                Where-Object { $BuildOnlyExtensions -contains $_.Extension.ToLowerInvariant() } |
                Remove-Item -Force

            # The build drops the plain Arm64 SDK next to the exe; the shipping one is Arm64X.
            foreach ($ext in @('dll', 'pri')) {
                Copy-Staged -Source (Join-Path $sdkBinFolder "Windows.Devices.Midi2.$ext") -Destination $toolStaging
            }

            $xbf = @(Get-ChildItem $toolStaging -Recurse -File -Filter '*.xbf')
            Write-Detail "  $($tool.Folder): $($xbf.Count) xbf, $(@(Get-ChildItem $toolStaging -File).Count) files in root"
        }

        Write-Detail "Staged SDK, $($ConsoleTools.Count) console tools, $($GuiTools.Count) GUI tools -> app-sdk\$plat"

        # --- MIDI Console -------------------------------------------------------------------
        $consoleStaging = Join-Path $StagingRoot "midi-console\$plat"
        Publish-DotNetApp -Project $ConsoleProject -BuildPlatform $plat -RuntimeIdentifier $rid `
            -Destination $consoleStaging -Version $Version

        Move-StagedSymbols -Folder $consoleStaging -BuildPlatform $plat

        # --- PowerShell module --------------------------------------------------------------
        $psStaging = Join-Path $StagingRoot "midi-powershell\$plat"

        Publish-DotNetApp -Project $PowerShellProject -BuildPlatform $plat -RuntimeIdentifier $rid `
            -Destination $psStaging -Version $Version

        Move-StagedSymbols -Folder $psStaging -BuildPlatform $plat

        $manifest = Join-Path $psStaging 'WindowsMidiServices.psd1'
        Copy-Item (Join-Path (Split-Path -Parent $PowerShellProject) 'WindowsMidiServices.psd1') `
            -Destination $manifest -Force

        # ModuleVersion must parse as System.Version, so the SemVer prerelease tag cannot be used
        # here. Stamped on the staged copy so the source manifest stays untouched.
        $psd1 = Get-Content $manifest -Raw
        $psd1 = [regex]::Replace($psd1, "(?m)^(\s*ModuleVersion\s*=\s*)'[^']*'", "`${1}'$($Version.NumericVersion)'", 1)
        Set-Content -Path $manifest -Value $psd1 -Encoding UTF8
    }

    # --- Shared design assets ---------------------------------------------------------------
    $transportAssets = Join-Path $StagingRoot 'Assets\Transports'
    $endpointAssets = Join-Path $StagingRoot 'Assets\Endpoints'
    New-Item -ItemType Directory -Force -Path $transportAssets, $endpointAssets | Out-Null

    Get-ChildItem $DesignRoot -Filter '*-small.svg' -File |
        ForEach-Object {
            $dest = if ($_.Name -like 'default-*') { $endpointAssets } else { $transportAssets }
            Copy-Item $_.FullName -Destination $dest -Force
        }

    Write-Detail "Staged transport and endpoint assets"

    # --- CollectMidiLogs ---------------------------------------------------------------------
    $logsStaging = Join-Path $StagingRoot 'CollectMidiLogs'
    foreach ($f in @('CollectMidiLogs.cmd', 'CollectMidiLogs.ps1', 'providers.wprp', 'tttraceall.psm1')) {
        Copy-Staged -Source (Join-Path $CollectMidiLogsRoot $f) -Destination $logsStaging
    }
    Write-Detail 'Staged CollectMidiLogs'
}

# ----------------------------------------------------------------------------------------------
# WiX file list generation
#
# The old build hand-listed ~90 files across the .wxs files and a 700-line generator in Build.cs.
# Instead, walk what actually got staged and emit one ComponentGroup per package.
# ----------------------------------------------------------------------------------------------

function Get-WixStableId {
    param([string] $Prefix, [string] $Value)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = $sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($Value))
    }
    finally {
        $sha.Dispose()
    }
    # MSI Identifiers are capped at 72 chars and many package file names are long, so hash rather
    # than concatenate. WiX derives the auto component GUID from the install path, not this Id,
    # so ids may change freely without breaking upgrades.
    return $Prefix + [System.BitConverter]::ToString($bytes[0..7]).Replace('-', '')
}

function ConvertTo-WixId {
    param([string] $Prefix, [string] $RelativePath)
    $clean = ($RelativePath -replace '\$\(var\.Platform\)', '' -replace '[^A-Za-z0-9]', '_') -replace '_+', '_'
    return ($Prefix + '_' + $clean).Trim('_')
}

function New-WixFileListFragment {
    param(
        [Parameter(Mandatory)] [string] $OutputFile,
        [Parameter(Mandatory)] [string] $ComponentGroupId,
        # Ordered map of staging-relative folder -> MSI Directory Id. A folder listed here is
        # emitted flat; add it to -RecurseRoots to walk its subfolders as well.
        [Parameter(Mandatory)] [System.Collections.Specialized.OrderedDictionary] $DirectoryMap,
        [string[]] $RecurseRoots = @(),
        [hashtable] $FileIdOverrides = @{}
    )

    $directoryDeclarations = [System.Text.StringBuilder]::new()
    $components = [System.Text.StringBuilder]::new()
    $script:ComponentIndex = 0

    function Add-Component {
        param([string] $ProbeFolder, [string] $RelativeFolder, [string] $DirectoryId)

        $files = @(Get-ChildItem $ProbeFolder -File | Sort-Object Name)
        if ($files.Count -eq 0) { return }

        [void]$components.AppendLine('')
        [void]$components.AppendLine("      <!-- $RelativeFolder -->")

        foreach ($file in $files) {
            $script:ComponentIndex++

            $source = '$(StagingSourceRootFolder)\' + $RelativeFolder + '\' + $file.Name
            $key = "$DirectoryId|$($file.Name)"

            $componentId = Get-WixStableId -Prefix 'c' -Value $key
            $fileId = if ($FileIdOverrides.ContainsKey($file.Name)) {
                $FileIdOverrides[$file.Name]
            }
            else {
                Get-WixStableId -Prefix 'f' -Value $key
            }

            # One file per component: that is the MSI guidance, and it is also the only shape
            # WiX will generate a component GUID for automatically.
            [void]$components.AppendLine("      <Component Id=`"$componentId`" Bitness=`"always64`" Directory=`"$DirectoryId`"> <!-- $($file.Name) -->")
            [void]$components.AppendLine("        <File Id=`"$fileId`" Source=`"$source`" Vital=`"true`" />")
            [void]$components.AppendLine('      </Component>')
        }
    }

    # Walks staged subfolders, declaring an MSI Directory for each so new content folders are
    # picked up without touching the .wxs by hand.
    function Add-SubTree {
        param([string] $ProbeFolder, [string] $RelativeFolder, [string] $ParentId, [int] $Indent)

        $pad = ' ' * $Indent

        foreach ($sub in (Get-ChildItem $ProbeFolder -Directory | Sort-Object Name)) {
            $childRelative = "$RelativeFolder\$($sub.Name)"
            $childId = ConvertTo-WixId -Prefix $ParentId -RelativePath $sub.Name

            $hasContent = @(Get-ChildItem $sub.FullName -File -Recurse).Count -gt 0
            if (-not $hasContent) { continue }

            [void]$directoryDeclarations.AppendLine("$pad<Directory Id=`"$childId`" Name=`"$($sub.Name)`">")
            Add-SubTree -ProbeFolder $sub.FullName -RelativeFolder $childRelative -ParentId $childId -Indent ($Indent + 2)
            [void]$directoryDeclarations.AppendLine("$pad</Directory>")

            Add-Component -ProbeFolder $sub.FullName -RelativeFolder $childRelative -DirectoryId $childId
        }
    }

    foreach ($relativeFolder in $DirectoryMap.Keys) {
        $directoryId = $DirectoryMap[$relativeFolder]
        $probeFolder = Join-Path $StagingRoot ($relativeFolder -replace '\$\(var\.Platform\)', $script:CurrentWixPlatform)

        if (-not (Test-Path $probeFolder)) {
            Write-Note "No staged content for $relativeFolder - skipping"
            continue
        }

        Add-Component -ProbeFolder $probeFolder -RelativeFolder $relativeFolder -DirectoryId $directoryId

        if ($RecurseRoots -contains $relativeFolder) {
            [void]$directoryDeclarations.AppendLine("    <DirectoryRef Id=`"$directoryId`">")
            Add-SubTree -ProbeFolder $probeFolder -RelativeFolder $relativeFolder -ParentId $directoryId -Indent 6
            [void]$directoryDeclarations.AppendLine('    </DirectoryRef>')
        }
        else {
            foreach ($sub in (Get-ChildItem $probeFolder -Directory)) {
                $subRelative = "$relativeFolder\$($sub.Name)"
                if (-not $DirectoryMap.Contains($subRelative)) {
                    Write-Note "Staged folder is not mapped to an MSI directory and will NOT be installed: $subRelative"
                }
            }
        }
    }

    $sb = [System.Text.StringBuilder]::new()
    [void]$sb.AppendLine('<?xml version="1.0" encoding="utf-8"?>')
    [void]$sb.AppendLine('<!-- Generated by build\build-sdk.ps1. Do not edit; your changes will be overwritten. -->')
    [void]$sb.AppendLine('<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">')
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('  <?define StagingSourceRootFolder=$(env.MIDI_REPO_ROOT)\build\staging ?>')
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('  <Fragment>')
    if ($directoryDeclarations.Length -gt 0) {
        [void]$sb.Append($directoryDeclarations.ToString())
        [void]$sb.AppendLine('')
    }
    [void]$sb.AppendLine("    <ComponentGroup Id=`"$ComponentGroupId`">")
    [void]$sb.Append($components.ToString())
    [void]$sb.AppendLine('    </ComponentGroup>')
    [void]$sb.AppendLine('  </Fragment>')
    [void]$sb.AppendLine('</Wix>')

    Set-Content -Path $OutputFile -Value $sb.ToString() -Encoding UTF8
    Write-Detail "Generated $(Split-Path -Leaf $OutputFile) ($($script:ComponentIndex) files)"
}

function New-StartMenuFragment {
    param([Parameter(Mandatory)] [string] $OutputFile)

    $sb = [System.Text.StringBuilder]::new()
    [void]$sb.AppendLine('<?xml version="1.0" encoding="utf-8"?>')
    [void]$sb.AppendLine('<!-- Generated by build\build-sdk.ps1. Do not edit; your changes will be overwritten. -->')
    [void]$sb.AppendLine('<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">')
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('  <Fragment>')
    [void]$sb.AppendLine('    <StandardDirectory Id="ProgramMenuFolder">')
    [void]$sb.AppendLine("      <Directory Id=`"MIDI_PROGRAMS_FOLDER`" Name=`"$StartMenuFolderName`" />")
    [void]$sb.AppendLine('    </StandardDirectory>')
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('    <ComponentGroup Id="ToolAppShortcuts">')
    [void]$sb.AppendLine('      <Component Id="ToolAppShortcutsComponent" Bitness="always64" Directory="MIDI_PROGRAMS_FOLDER" Guid="0d1b7b1e-3a5e-4a2f-9a3c-6f2b6c4d5e71">')

    foreach ($tool in $GuiTools) {
        [void]$sb.AppendLine("        <Shortcut Id=`"Shortcut_$($tool.Name)`"")
        [void]$sb.AppendLine("                  Name=`"$($tool.Display)`"")
        [void]$sb.AppendLine("                  Target=`"[#$($tool.Name)Exe]`"")
        [void]$sb.AppendLine("                  WorkingDirectory=`"$($tool.DirectoryId)`" />")
    }

    [void]$sb.AppendLine('        <RemoveFolder Id="RemoveMidiProgramsFolder_Tools" Directory="MIDI_PROGRAMS_FOLDER" On="uninstall" />')
    [void]$sb.AppendLine('        <RegistryKey Root="HKLM" Key="SOFTWARE\Microsoft\Windows MIDI Services\Desktop App SDK Runtime">')
    [void]$sb.AppendLine('          <RegistryValue Type="string" Name="ToolAppShortcuts" Value="installed" KeyPath="yes" />')
    [void]$sb.AppendLine('        </RegistryKey>')
    [void]$sb.AppendLine('      </Component>')
    [void]$sb.AppendLine('    </ComponentGroup>')
    [void]$sb.AppendLine('  </Fragment>')
    [void]$sb.AppendLine('</Wix>')

    Set-Content -Path $OutputFile -Value $sb.ToString() -Encoding UTF8
    Write-Detail "Generated $(Split-Path -Leaf $OutputFile) ($($GuiTools.Count) shortcuts)"
}

function New-SetupFileLists {
    # The generated lists are platform-independent in content (same file names for x64/Arm64),
    # so enumerate using whichever platform we actually built.
    $script:CurrentWixPlatform = if ($Platform -contains 'x64') { 'x64' } else { 'Arm64' }

    # --- SDK runtime + in-box tools ---
    $sdkDirs = [ordered]@{ 'app-sdk\$(var.Platform)' = 'TOOLSROOT_INSTALLFOLDER' }
    $sdkFileIds = @{ 'mididiag.exe' = 'MidiDiagExe' }
    $sdkRecurse = @()

    foreach ($tool in $GuiTools) {
        $toolFolder = 'app-sdk\$(var.Platform)\' + $tool.Folder
        $sdkDirs[$toolFolder] = $tool.DirectoryId
        # WinUI apps keep .xbf in subfolders (e.g. Styles\), and that layout has to survive
        # into the install folder or XAML resource lookup fails at runtime.
        $sdkRecurse += $toolFolder
        # Fixed Id so the Start Menu shortcut can target it with [#Id].
        $sdkFileIds["$($tool.Name).exe"] = "$($tool.Name)Exe"
    }

    $sdkDirs['CollectMidiLogs'] = 'COLLECTMIDILOGS_INSTALLFOLDER'

    # The shared endpoint and transport art lives under ProgramData rather than Program Files,
    # because every MIDI app reads it and the customer's own pictures land beside it.
    $sdkDirs['Assets\Endpoints'] = 'CONFIGURATION_ASSETS_ENDPOINTS_FOLDER'
    $sdkDirs['Assets\Transports'] = 'CONFIGURATION_ASSETS_TRANSPORTS_FOLDER'

    New-WixFileListFragment `
        -OutputFile (Join-Path $SetupSolutionRoot 'sdk-package\_SetupFiles.wxs') `
        -ComponentGroupId 'SdkRedistFiles' `
        -DirectoryMap $sdkDirs `
        -RecurseRoots $sdkRecurse `
        -FileIdOverrides $sdkFileIds

    New-StartMenuFragment -OutputFile (Join-Path $SetupSolutionRoot 'sdk-package\_StartMenu.wxs')

    # --- MIDI Console ---
    $consoleDirs = [ordered]@{ 'midi-console\$(var.Platform)' = 'CONSOLEAPP_INSTALLFOLDER' }
    New-WixFileListFragment `
        -OutputFile (Join-Path $SetupSolutionRoot 'console-package\_SetupFiles.wxs') `
        -ComponentGroupId 'ConsoleAppFiles' `
        -DirectoryMap $consoleDirs `
        -FileIdOverrides @{ 'midi.exe' = 'MidiConsoleExe' }

    $psDirs = [ordered]@{ 'midi-powershell\$(var.Platform)' = 'POWERSHELL_MODULE_INSTALLFOLDER' }
    New-WixFileListFragment `
        -OutputFile (Join-Path $SetupSolutionRoot 'powershell-package\_SetupFiles.wxs') `
        -ComponentGroupId 'PowerShellModuleFiles' `
        -DirectoryMap $psDirs
}

function Get-InstallerPath {
    param([Parameter(Mandatory)] [string] $BuildPlatform)
    Join-Path $SetupSolutionRoot "main-bundle\bin\$BuildPlatform\$Configuration\WindowsMidiServicesSdkRuntimeSetup.exe"
}

function Invoke-SetupTarget {
    param($Version)

    Write-Step 'Setup'

    New-SetupFileLists

    foreach ($plat in $Platform) {
        Invoke-MSBuild -ProjectOrSolution $SetupSolution -BuildPlatform $plat `
            -Targets @('Restore', 'Rebuild')

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

    $folder = Join-Path $ReleaseRoot $Version.ReleaseLabel
    New-Item -ItemType Directory -Force -Path $folder | Out-Null

    $pkg = Join-Path $SdkNuGetOutput "$($Version.NuGetPackageId).$($Version.SemVer).nupkg"
    if (Test-Path $pkg) {
        Copy-Item $pkg -Destination $folder -Force
        Write-Detail "NuGet package -> $(Split-Path -Leaf $pkg)"
    }
    else {
        Write-Note "NuGet package not found: $pkg"
    }

    foreach ($plat in $Platform) {
        $bundle = Get-InstallerPath -BuildPlatform $plat
        if (-not (Test-Path $bundle)) {
            Write-Note "Installer not found for $plat - run the Setup target: $bundle"
            continue
        }

        $name = "Windows MIDI Services Tools $($Version.SemVer)-$($plat.ToLowerInvariant()).exe"
        Copy-Item $bundle -Destination (Join-Path $folder $name) -Force
        Write-Detail "Installer -> $name"
    }

    # A crash report gives a module name, a build timestamp and an offset. Without the pdb from
    # that exact build the offset cannot be turned back into a function, and the build output is
    # overwritten by the next build. This is the only copy that survives, because nothing here is
    # installed on a customer's PC.
    foreach ($plat in $Platform) {
        $symbolFolder = Join-Path $folder "symbols\$plat"
        New-Item -ItemType Directory -Force -Path $symbolFolder | Out-Null

        $sdkBinarySourcePlatform = if ($plat -eq 'Arm64') { 'Arm64EC' } else { $plat }

        $sources = @(Join-Path $SdkOutRoot "Windows.Devices.Midi2\$sdkBinarySourcePlatform\$Configuration\Windows.Devices.Midi2.pdb")

        foreach ($tool in $ConsoleTools) {
            $sources += Join-Path $SdkOutRoot "$tool\$plat\$Configuration\$tool.pdb"
        }

        foreach ($tool in $GuiTools) {
            $sources += Join-Path $SdkOutRoot "$($tool.Name)\$plat\$Configuration\$($tool.Name).pdb"
        }

        # The managed packages publish into staging, so their symbols were set aside there rather
        # than being looked for under a per-project output path.
        $stagedSymbols = Join-Path $StagingRoot "symbols\$plat"
        if (Test-Path $stagedSymbols) {
            $sources += @(Get-ChildItem $stagedSymbols -File -Filter '*.pdb' | Select-Object -ExpandProperty FullName)
        }
        else {
            Write-Note "No staged symbols for $plat - run the Stage target"
        }

        $copied = 0

        foreach ($source in $sources) {
            if (Test-Path $source) {
                Copy-Item $source -Destination $symbolFolder -Force
                $copied++
            }
            else {
                Write-Note "Symbols not found: $source"
            }
        }

        Write-Detail "Symbols -> symbols\$plat ($copied files)"
    }

    Write-Host ''
    Write-Host "     Release folder: $folder" -ForegroundColor Green
}

# ----------------------------------------------------------------------------------------------
# Clean
# ----------------------------------------------------------------------------------------------

function Invoke-CleanTarget {
    Write-Step 'Clean'

    foreach ($path in @($StagingRoot, (Join-Path $ApiRoot 'vsfiles-sdk\out'), (Join-Path $ApiRoot 'vsfiles-sdk\intermediate'))) {
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

$targets = if ($Target -contains 'All') { @('Version', 'Sdk', 'Stage', 'Setup', 'Release') } else { $Target }

Write-Host ''
Write-Host 'Windows MIDI Services - App SDK build' -ForegroundColor White
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
    # Later targets still need version strings even when not regenerating the version files.
    Write-Detail "Version       $($version.SemVer)"
}

if ($targets -contains 'Sdk' -or $targets -contains 'Setup' -or $targets -contains 'Stage') {
    $script:MSBuild = Resolve-MSBuild
    Write-Detail "MSBuild       $script:MSBuild"
}

if ($targets -contains 'Version') { Invoke-VersionTarget $version }
if ($targets -contains 'Sdk') { Invoke-SdkTarget $version }
if ($targets -contains 'Stage') { Invoke-StageTarget $version }
if ($targets -contains 'Setup') { Invoke-SetupTarget $version }
if ($targets -contains 'Release') { Invoke-ReleaseTarget $version }

$stopwatch.Stop()
Write-Host ''
Write-Host ("Done in {0:hh\:mm\:ss}" -f $stopwatch.Elapsed) -ForegroundColor Green
Write-Host ''

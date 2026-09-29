<#
.SYNOPSIS
    Builds, stages, signs and packages everything Windows MIDI Services releases from GitHub: the
    WinRT SDK and its NuGet package, the Tools installer, MIDI Glass, and the transport installers.

.DESCRIPTION
    One pass builds each solution once per platform:

      src/in-box/Midi2.sln          the transports: Network MIDI 2.0, RTP-MIDI, Basic Loopback,
                                    General MIDI Synthesizer and Bluetooth MIDI
      src/in-box/Midi2-AppSDK.sln   the SDK, every app and tool including MIDI Glass, and the
                                    NuGet projection

    The installers stay separate, and so do the version numbers:

      build/version.json              the SDK, the NuGet package, the samples and the Tools installer
      build/version-plugins.json      the transport installers: Network MIDI 2.0 and RTP-MIDI,
                                      Bluetooth MIDI, Basic Loopback and General MIDI Synthesizer
      build/version-midi-glass.json   MIDI Glass

    Every app installs into one folder, Program Files\Windows MIDI Services\Tools, the way they are
    laid out in Windows itself. The transport-specific setup apps still come from the installer
    that carries their transport. Sharing a folder works because this script makes sure that:

      - each WinUI app keeps its resources in <exe>.pri instead of a shared resources.pri, and its
        XAML is compiled into that file, so no .xbf file is installed for another app to overwrite;
      - any file two apps both carry is byte for byte the same file;
      - every installer that puts a file in the Tools folder gives it the same component GUID,
        worked out from its install path, so Windows Installer keeps count of it and removing one
        installer leaves it in place for the others;
      - every copy of Windows.Devices.Midi2.dll and .pri is the one in the NuGet package: the x64
        build for x64, and the Arm64X build for Arm64.

.PARAMETER Target
    One or more of:
      Version  Work out all three versions, write the WiX version includes and the version
               headers, and stamp the NuGet nuspec.
      Service  Build src/in-box/Midi2.sln for each platform (the transports).
      Sdk      Build src/in-box/Midi2-AppSDK.sln for each platform, plus Arm64EC for the Arm64X
               SDK binary, then check every SDK binary that ships is the right architecture and
               was linked by this build.
      Pack     Sign (with -Sign) and pack the NuGet package once every platform is built, then
               check what went into it. Needs Sdk in the same run, for x64 and Arm64.
      Samples  Point the samples at the new package, compile them and zip their source.
      Stage    Copy everything that ships into build/staging, checking that the apps can share
               one folder.
      Setup    Generate the installers' file lists and build every installer.
      Release  Collect the installers, the package, the samples and the symbols into build/release.
      Clean    Delete staging and the build output folders.
      All      Version, Service, Sdk, Pack, Samples, Stage, Setup, Release.

.PARAMETER BuildNumber
    Overrides the 'build' field of all three version files without changing them. For CI, for
    example -BuildNumber $env:GITHUB_RUN_NUMBER.

.PARAMETER BumpBuildNumber
    Increments and saves the 'build' field of all three version files before anything else runs.

.PARAMETER Sign
    Authenticode-sign everything that ships: the staged binaries, the SDK binaries the NuGet
    package carries, the installer custom actions, the MSI packages and the installer bundles.
    Needs the Artifact Signing client tools and a signed-in identity; see build/sign-files.ps1.

.PARAMETER SigningMetadata
    The Artifact Signing metadata JSON used by -Sign. Defaults to MIDI_SIGNING_METADATA.

.PARAMETER MaxCpuCount
    How many projects MSBuild builds at once. Defaults to three quarters of the logical
    processors so the machine stays usable while a build runs. 0 uses every logical processor,
    which is a little faster and makes the desktop stutter.

.PARAMETER Priority
    Process priority for MSBuild and the compilers, which inherit it from this script.
    BelowNormal (the default) keeps the UI responsive. Use Normal on a build machine.

.EXAMPLE
    .\build-all.ps1
    Full release build for x64 and Arm64.

.EXAMPLE
    .\build-all.ps1 -Sign -SigningMetadata C:\signing\metadata.json
    Full release build, signed with an Azure Artifact Signing certificate profile.

.EXAMPLE
    .\build-all.ps1 -Target Stage,Setup -Platform x64
    Restage what was last built and rebuild the x64 installers.
#>
[CmdletBinding()]
param(
    # Comma-separated is accepted as a single token so this works through `pwsh -File` and
    # build-all.cmd, which do not split array arguments.
    [string[]] $Target = @('All'),

    [string[]] $Platform = @('x64', 'Arm64'),

    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Release',

    [int] $BuildNumber = -1,

    [switch] $BumpBuildNumber,

    [switch] $Sign,

    [string] $SigningMetadata = $env:MIDI_SIGNING_METADATA,

    # Explicit MSBuild.exe. Leave empty to let vswhere find the newest install.
    [string] $MSBuildPath,

    # Parallel MSBuild nodes. 0 means one per logical processor.
    [ValidateRange(0, 256)]
    [int] $MaxCpuCount = [Math]::Max(1, [int][Math]::Floor([Environment]::ProcessorCount * 0.75)),

    [ValidateSet('Normal', 'BelowNormal', 'Idle')]
    [string] $Priority = 'BelowNormal',

    [ValidateSet('quiet', 'minimal', 'normal', 'detailed', 'diagnostic')]
    [string] $Verbosity = 'minimal'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$InformationPreference = 'Continue'

$AllTargets = @('Version', 'Service', 'Sdk', 'Pack', 'Samples', 'Stage', 'Setup', 'Release')

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

$Target = Expand-Argument -Value $Target -Allowed (@('All', 'Clean') + $AllTargets) -Name 'Target'
$Platform = Expand-Argument -Value $Platform -Allowed @('x64', 'Arm64') -Name 'Platform'

# ----------------------------------------------------------------------------------------------
# Paths
# ----------------------------------------------------------------------------------------------

$BuildRoot = $PSScriptRoot
$RepoRoot = Split-Path -Parent $BuildRoot

$SourceRoot = Join-Path $RepoRoot 'src'
$ApiRoot = Join-Path $SourceRoot 'in-box'
$UserToolsRoot = Join-Path $ApiRoot 'user-tools'
$InstallersRoot = Join-Path $SourceRoot 'installers'

$ServiceSolution = Join-Path $ApiRoot 'Midi2.sln'
$ServiceOutRoot = Join-Path $ApiRoot 'VSFiles'

$SdkSolution = Join-Path $ApiRoot 'Midi2-AppSDK.sln'
$SdkOutRoot = Join-Path $ApiRoot 'vsfiles-sdk\out'
$SdkIntermediateRoot = Join-Path $ApiRoot 'vsfiles-sdk\intermediate'
$SdkNuGetOutput = Join-Path $ApiRoot 'vsfiles-sdk\PublishedNuGet'

$NuspecFile = Join-Path $ApiRoot 'Client\WinRT\NuGet\Windows.Devices.Midi2.NuGet\nuget\Windows.Devices.Midi2.nuspec'
$NuGetProject = Join-Path $ApiRoot 'Client\WinRT\NuGet\Windows.Devices.Midi2.NuGet\Windows.Devices.Midi2.NuGet.csproj'

$PowerShellProject = Join-Path $ApiRoot 'Client\WinRT\powershell\WindowsMidiServices.csproj'

$DesignRoot = Join-Path $RepoRoot 'design'
$CollectMidiLogsRoot = Join-Path $ApiRoot 'CollectMidiLogs'

$SamplesRoot = Join-Path $RepoRoot 'samples'

$StagingRoot = Join-Path $BuildRoot 'staging'
$ReleaseRoot = Join-Path $BuildRoot 'release'
$VersionStagingFolder = Join-Path $StagingRoot 'version'
$TransportStagingRoot = Join-Path $StagingRoot 'api'

# Each version file drives its own WiX include, so the installers built from one of them never
# pick up another's version number.
$SdkVersionFile = Join-Path $BuildRoot 'version.json'
$PluginsVersionFile = Join-Path $BuildRoot 'version-plugins.json'
$GlassVersionFile = Join-Path $BuildRoot 'version-midi-glass.json'

$SdkVersionInclude = Join-Path $VersionStagingFolder 'AppSdkVersion.wxi'
$PluginsVersionInclude = Join-Path $VersionStagingFolder 'BundleInfo.wxi'
$GlassVersionInclude = Join-Path $VersionStagingFolder 'MidiGlassVersion.wxi'

$SignScript = Join-Path $BuildRoot 'sign-files.ps1'

$ApiReferenceRoot = Join-Path $SourceRoot 'shared\api-ref'
$ApiIncludeFolder = Join-Path $ApiRoot 'Inc'
$NetworkTransportFolder = Join-Path $ApiRoot 'Transport\UdpNetworkMidi2Transport'

# The .wxs files resolve staging as "$(env.MIDI_REPO_ROOT)\build\staging", so this must NOT have
# a trailing separator.
$env:MIDI_REPO_ROOT = $RepoRoot.TrimEnd('\')

# ----------------------------------------------------------------------------------------------
# What ships
# ----------------------------------------------------------------------------------------------

# In-box console tools in the Tools installer. Name = project name = exe name.
$ConsoleTools = @(
    'mididiag'
    'midiksinfo'
    'midi1monitor'
    'midi1enum'
)

# In-box GUI tools in the Tools installer.
# Display is the Start Menu shortcut name, and it DELIBERATELY drops the "Windows" the app's own
# title bar carries. The Start Menu gives a tile about a dozen characters before it elides, so
# "Windows MIDI Player" showed as "Windows MIDI..." and the only part that identified the app was
# the part that got cut. The group these all sit in is already called Windows MIDI (Preview).
# Network MIDI 2.0 Setup and Bluetooth MIDI Setup are deliberately NOT here: each ships in the
# installer that carries its transport, because the app is useless without it. MIDI Glass has an
# installer of its own.
$GuiTools = @(
    [pscustomobject]@{ Name = 'midisettings';       Display = 'MIDI Settings' }
    [pscustomobject]@{ Name = 'midiloopbacksetup';  Display = 'MIDI Loopback Setup' }
    [pscustomobject]@{ Name = 'midiscratchpad';     Display = 'MIDI Scratch Pad' }
    [pscustomobject]@{ Name = 'midikeyboard';       Display = 'MIDI Keyboard' }
    [pscustomobject]@{ Name = 'midiplayer';         Display = 'MIDI Player' }
    [pscustomobject]@{ Name = 'midiclock';          Display = 'MIDI Clock' }
    [pscustomobject]@{ Name = 'midisysextool';      Display = 'MIDI SysEx Tool' }
    [pscustomobject]@{ Name = 'midi2monitor';       Display = 'MIDI Monitor' }
    [pscustomobject]@{ Name = 'midipatchbay';       Display = 'MIDI Patchbay' }
    [pscustomobject]@{ Name = 'miditroubleshooter'; Display = 'MIDI Troubleshooting and Repair' }
    # Aumid: the notification platform will not accept a toast from an unpackaged app unless the
    # identity it publishes under is on a Start Menu shortcut. RunAtLogon means the installer
    # PRESERVES an existing machine wide Run entry across an upgrade - it never creates one. A
    # tray app that nobody asked for is not something to install by default; MIDI Settings owns
    # turning it on, per user or for everyone.
    [pscustomobject]@{ Name = 'midinotifications'; Display = 'MIDI Notifications'; Aumid = 'Microsoft.WindowsMidiServices.Notifications'; RunAtLogon = $true }
)

# The MSI Directory every installer uses for Program Files\Windows MIDI Services\Tools.
$ToolsDirectoryId = 'TOOLSROOT_INSTALLFOLDER'

# Every file installed into the Tools folder gets a component GUID made from this and its path
# under Program Files, so every installer that carries the file owns the same component. Windows
# Installer then keeps count, and removing one installer leaves the file for the others. The
# installers are built with two different WiX versions, so this does not rely on WiX's own
# generated GUIDs agreeing. Never change it: every shared file would get a new GUID.
$ToolsComponentGuidNamespace = [guid]'ef756d1b-877d-48c9-830a-02f7cb471a69'

# The GUIDs WiX generated for these two when only the Tools installer put them in this folder
# (through SDK Dev Preview 10). Keeping them makes an older Tools install count as an owner too,
# so uninstalling it cannot delete the SDK from under the other installers' apps.
$ToolsComponentGuidOverrides = @{
    'windows.devices.midi2.dll' = '49839909-91e7-51dc-86bc-fe09f392f246'
    'windows.devices.midi2.pri' = 'a6448708-c0b1-5e9c-b651-c6aa09d686c3'
}

# The installers that put apps into the Tools folder, and which apps each one carries. Staged to
# build/staging/<Name>/<platform>.
$ToolsFolderPayloads = @(
    [pscustomobject]@{
        Name           = 'app-sdk'
        Installer      = 'Tools'
        Apps           = @($ConsoleTools) + @($GuiTools | ForEach-Object { $_.Name })
        Fragment       = Join-Path $InstallersRoot 'api-and-tools-installer\sdk-package\_SetupFiles.wxs'
        ComponentGroup = 'SdkRedistFiles'
    }
    [pscustomobject]@{
        Name           = 'midi-console'
        Installer      = 'Tools'
        Apps           = @('midi')
        Fragment       = Join-Path $InstallersRoot 'api-and-tools-installer\console-package\_SetupFiles.wxs'
        ComponentGroup = 'ConsoleAppFiles'
    }
    [pscustomobject]@{
        Name           = 'midi-glass'
        Installer      = 'Glass'
        Apps           = @('midiglass')
        Fragment       = Join-Path $InstallersRoot 'midi-glass-installer\app-package\_AppFiles.wxs'
        ComponentGroup = 'MidiGlassFiles'
    }
    [pscustomobject]@{
        Name           = 'network-app'
        Installer      = 'Network'
        Apps           = @('midinetworksetup')
        Fragment       = Join-Path $InstallersRoot 'oob-setup-network\api-package\_AppFiles.wxs'
        ComponentGroup = 'NetworkSetupAppFiles'
    }
    [pscustomobject]@{
        Name           = 'bluetooth-app'
        Installer      = 'Bluetooth'
        Apps           = @('midibluetoothsetup')
        Fragment       = Join-Path $InstallersRoot 'oob-setup-bluetooth\api-package\_AppFiles.wxs'
        ComponentGroup = 'BluetoothSetupAppFiles'
    }
)

# Files an app needs beyond its exe, checked at Stage so a missing one fails the build rather than
# the app on a customer's PC. Glass draws its surface controls with the templates in
# Themes\Generic.xbf, which ships inside midiglass.pri.
$AppRequiredFiles = @{
    'midiglass' = @('Microsoft.Graphics.Canvas.dll', 'Themes\Generic.xbf')
}

# Transports, built by Midi2.sln and staged to build/staging/api/<platform>.
$Transports = @(
    'Midi2.NetworkMidiTransport'
    'Midi2.RtpMidiTransport'
    'Midi2.BasicLoopbackMidiTransport'
    'Midi2.MidiSynthTransport'
    'Midi2.BluetoothMidiTransport'
)

# Every installer. Train picks the version file. ReleaseFolder is formatted with that version's
# folder label. Apps and Transports are only used to collect the symbols for the release.
$Installers = @(
    [pscustomobject]@{
        Name          = 'Tools'
        Train         = 'Sdk'
        SolutionDir   = Join-Path $InstallersRoot 'api-and-tools-installer'
        Solution      = 'midi-services-app-sdk-runtime-setup.sln'
        BundleName    = 'WindowsMidiServicesSdkRuntimeSetup'
        ReleaseName   = 'Windows MIDI Services Tools'
        ReleaseFolder = '{0}'
        Apps          = @($ConsoleTools) + @($GuiTools | ForEach-Object { $_.Name }) + @('midi')
        Transports    = @()
    }
    [pscustomobject]@{
        Name          = 'Glass'
        Train         = 'Glass'
        SolutionDir   = Join-Path $InstallersRoot 'midi-glass-installer'
        Solution      = 'midi-glass-setup.sln'
        BundleName    = 'WindowsMidiServicesMidiGlassSetup'
        ReleaseName   = 'MIDI Glass'
        ReleaseFolder = 'midi-glass-{0}'
        Apps          = @('midiglass')
        Transports    = @()
    }
    [pscustomobject]@{
        Name          = 'Network'
        Train         = 'Plugins'
        SolutionDir   = Join-Path $InstallersRoot 'oob-setup-network'
        Solution      = 'midi-services-network-midi-preview-setup.sln'
        BundleName    = 'WindowsMidiServicesNetworkMidiSetup'
        ReleaseName   = 'Windows MIDI Services (Network MIDI 2.0 and RTP-MIDI Preview)'
        ReleaseFolder = 'plugins-{0}'
        Apps          = @('midinetworksetup')
        Transports    = @('Midi2.NetworkMidiTransport', 'Midi2.RtpMidiTransport')
    }
    [pscustomobject]@{
        Name          = 'BasicLoopback'
        Train         = 'Plugins'
        SolutionDir   = Join-Path $InstallersRoot 'oob-setup-basic-loopback'
        Solution      = 'midi-services-basic-loopback-setup.sln'
        BundleName    = 'WindowsMidiServicesBasicLoopbackSetup'
        ReleaseName   = 'Windows MIDI Services (Basic MIDI 1.0 Loopback Preview)'
        ReleaseFolder = 'plugins-{0}'
        Apps          = @()
        Transports    = @('Midi2.BasicLoopbackMidiTransport')
    }
    [pscustomobject]@{
        Name          = 'Synth'
        Train         = 'Plugins'
        SolutionDir   = Join-Path $InstallersRoot 'oob-setup-synth'
        Solution      = 'midi-services-synth-setup.sln'
        BundleName    = 'WindowsMidiServicesSynthSetup'
        ReleaseName   = 'Windows MIDI Services (General MIDI Synthesizer Preview)'
        ReleaseFolder = 'plugins-{0}'
        Apps          = @()
        Transports    = @('Midi2.MidiSynthTransport')
    }
    [pscustomobject]@{
        Name          = 'Bluetooth'
        Train         = 'Plugins'
        SolutionDir   = Join-Path $InstallersRoot 'oob-setup-bluetooth'
        Solution      = 'midi-services-bluetooth-midi-preview-setup.sln'
        BundleName    = 'WindowsMidiServicesBluetoothMidiSetup'
        ReleaseName   = 'Windows MIDI Services (Bluetooth MIDI Preview)'
        ReleaseFolder = 'bluetooth-{0}'
        Apps          = @('midibluetoothsetup')
        Transports    = @('Midi2.BluetoothMidiTransport')
    }
)

# Start Menu group shared by every MIDI GUI app, including MIDI Settings.
$StartMenuFolderName = 'Windows MIDI (Preview)'

# Linker and metadata leftovers in a native project's output folder. Never shipped.
$BuildOnlyExtensions = @('.exp', '.lib', '.winmd', '.ipdb', '.iobj', '.pdb', '.ilk')

# Sample sets shipped as zips alongside the release. Each is rewritten to reference the NuGet
# package this build produced, then compiled - a sample that no longer builds against the SDK is
# a release blocker, not a documentation nit.
$SampleSets = @(
    [pscustomobject]@{ Name = 'C++/WinRT'; Folder = Join-Path $SamplesRoot 'cpp-winrt';  Solution = 'cpp-winrt-samples.sln';  ZipPrefix = 'cpp-winrt-samples' }
    [pscustomobject]@{ Name = 'C#';        Folder = Join-Path $SamplesRoot 'csharp-net'; Solution = 'csharp-net-samples.sln'; ZipPrefix = 'csharp-samples' }
)

# Build output and per-user state. The zips carry source, project files, config and readmes only.
$SampleExcludedFolders = @('bin', 'obj', 'intermediate', 'packages', '.vs', 'Generated Files', 'AppPackages', 'BundleArtifacts', 'TestResults')
$SampleExcludedExtensions = @('.user', '.log', '.zip', '.pfx', '.suo', '.cache', '.binlog', '.nupkg', '.opendb', '.db')

# Surfaced in the generated version headers and shown in midi2monitor's settings dialog.
$SdkBuildSource = 'GitHub Preview'

# Arm64EC is built only to produce the Arm64X SDK binary. These are Windows.Devices.Midi2 and the
# projects it declares as solution dependencies, in build order - static libs are not rebuilt by
# their dependents, so they have to come first.
#
# The first two exist only to emit generated headers the rest include out of their per-platform
# intermediate folders. Together with com-extensions-idl they cover every intermediate folder
# these projects include from, which is what makes this list complete on a clean tree.
$Arm64EcProjects = @(
    @{ Project = 'idl\IDL.vcxproj'; Targets = @() }

    # Midl only: the SDK core includes this project's generated header, but compiling its C++
    # needs the service's RPC stubs, which are never generated for Arm64EC.
    @{ Project = 'Transport\MidiSrvTransport\Midi2.MidiSrvTransport.vcxproj'; Targets = @('Midl') }

    @{ Project = 'Libs\SDK-MidiPluginConfigurationLib\MidiPluginConfigurationLib.vcxproj'; Targets = @() }
    @{ Project = 'Libs\SDK-MidiEndpointNamingLib\MidiEndpointNamingLib.vcxproj'; Targets = @() }
    @{ Project = 'Libs\SDK-MidiPnpLib\MidiPnpLib.vcxproj'; Targets = @() }
    @{ Project = 'Client\WinRT\com-extensions-idl\com-extensions-idl.vcxproj'; Targets = @() }
    @{ Project = 'Client\WinRT\core\Windows.Devices.Midi2.vcxproj'; Targets = @() }
)

# Headers consumers compile against. mididiag and midi2monitor put src/shared/api-ref/<platform>
# on their IncludePath, so the SDK solution cannot build on a clean clone without these.
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
# Small file helpers
# ----------------------------------------------------------------------------------------------

function Get-Sha256 {
    param([Parameter(Mandatory)] [string] $Path)
    (Get-FileHash -Path $Path -Algorithm SHA256).Hash
}

# Generated headers and fragments are rewritten only when their content changes. A new timestamp
# on an unchanged header recompiles everything that includes it.
function Set-ContentIfChanged {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [AllowEmptyString()] [string] $Value
    )

    # Same bytes as Set-Content -Encoding UTF8 writes: UTF-8 without a BOM, ending in a newline.
    $text = $Value + [Environment]::NewLine

    if ((Test-Path $Path) -and ([System.IO.File]::ReadAllText($Path) -ceq $text)) {
        return $false
    }

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    [System.IO.File]::WriteAllText($Path, $text, [System.Text.UTF8Encoding]::new($false))
    return $true
}

function Copy-IfChanged {
    param(
        [Parameter(Mandatory)] [string] $Source,
        [Parameter(Mandatory)] [string] $DestinationFolder
    )

    if (-not (Test-Path $Source)) { throw "Source file not found: $Source" }

    $destination = Join-Path $DestinationFolder (Split-Path -Leaf $Source)

    if ((Test-Path $destination) -and ((Get-Sha256 $destination) -eq (Get-Sha256 $Source))) {
        return
    }

    New-Item -ItemType Directory -Force -Path $DestinationFolder | Out-Null
    Copy-Item -Path $Source -Destination $destination -Force
}

function Copy-Staged {
    param(
        [Parameter(Mandatory)] [string] $Source,
        [Parameter(Mandatory)] [string] $Destination
    )

    if (-not (Test-Path $Source)) { throw "Staging source not found: $Source" }

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Copy-Item -Path $Source -Destination $Destination -Force
}

function New-EmptyFolder {
    param([Parameter(Mandatory)] [string] $Path)

    # A stale file left in a staging folder would be packaged without anyone noticing, so staging
    # folders are always rebuilt from nothing.
    if (Test-Path $Path) { Remove-Item $Path -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

# ----------------------------------------------------------------------------------------------
# Binary inspection
# ----------------------------------------------------------------------------------------------

# Reads the architecture from a PE image. An Arm64X image has the Arm64 machine type and also
# carries Arm64EC code, which shows up as a CHPE metadata pointer in its load configuration; an
# Arm64EC-only image has the x64 machine type and the same pointer.
function Get-PeArchitecture {
    param([Parameter(Mandatory)] [byte[]] $Image)

    if ($Image.Length -lt 0x40 -or $Image[0] -ne 0x4D -or $Image[1] -ne 0x5A) {
        throw 'Not a PE image: no MZ header'
    }

    $pe = [BitConverter]::ToInt32($Image, 0x3C)
    if ($pe -lt 0 -or $pe + 24 -gt $Image.Length -or [BitConverter]::ToUInt32($Image, $pe) -ne 0x00004550) {
        throw 'Not a PE image: no PE signature'
    }

    $machine = [BitConverter]::ToUInt16($Image, $pe + 4)
    $sectionCount = [BitConverter]::ToUInt16($Image, $pe + 6)
    $optionalHeaderSize = [BitConverter]::ToUInt16($Image, $pe + 20)
    $optionalHeader = $pe + 24

    $hybrid = $false

    # PE32+ only: the data directories start 112 bytes in, and the load configuration is entry 10.
    if ([BitConverter]::ToUInt16($Image, $optionalHeader) -eq 0x20B) {
        $loadConfigRva = [BitConverter]::ToUInt32($Image, $optionalHeader + 112 + (10 * 8))

        if ($loadConfigRva -ne 0) {
            $sectionTable = $optionalHeader + $optionalHeaderSize

            for ($i = 0; $i -lt $sectionCount; $i++) {
                $section = $sectionTable + ($i * 40)
                $virtualSize = [int64][BitConverter]::ToUInt32($Image, $section + 8)
                $virtualAddress = [int64][BitConverter]::ToUInt32($Image, $section + 12)
                $rawSize = [int64][BitConverter]::ToUInt32($Image, $section + 16)
                $rawPointer = [int64][BitConverter]::ToUInt32($Image, $section + 20)

                if ($loadConfigRva -ge $virtualAddress -and $loadConfigRva -lt ($virtualAddress + [Math]::Max($virtualSize, $rawSize))) {
                    $loadConfig = [int]($rawPointer + ($loadConfigRva - $virtualAddress))

                    # CHPEMetadataPointer is at 0xC8 in IMAGE_LOAD_CONFIG_DIRECTORY64.
                    if ([BitConverter]::ToUInt32($Image, $loadConfig) -ge 0xD0) {
                        $hybrid = [BitConverter]::ToUInt64($Image, $loadConfig + 0xC8) -ne 0
                    }
                    break
                }
            }
        }
    }

    switch ($machine) {
        0x8664 { if ($hybrid) { 'Arm64EC' } else { 'x64' } }
        0xAA64 { if ($hybrid) { 'Arm64X' } else { 'Arm64' } }
        0x014C { 'x86' }
        default { 'unknown machine 0x{0:X4}' -f $machine }
    }
}

function Get-FileArchitecture {
    param([Parameter(Mandatory)] [string] $Path)
    Get-PeArchitecture -Image ([System.IO.File]::ReadAllBytes($Path))
}

# RFC 4122 version 5: a GUID that is always the same for the same namespace and name.
function New-NameBasedGuid {
    param(
        [Parameter(Mandatory)] [guid] $Namespace,
        [Parameter(Mandatory)] [string] $Name
    )

    # The namespace is hashed in network byte order; .NET stores its first three fields little
    # endian.
    $namespaceBytes = $Namespace.ToByteArray()
    [Array]::Reverse($namespaceBytes, 0, 4)
    [Array]::Reverse($namespaceBytes, 4, 2)
    [Array]::Reverse($namespaceBytes, 6, 2)

    $data = [byte[]]($namespaceBytes + [System.Text.Encoding]::UTF8.GetBytes($Name))

    $sha1 = [System.Security.Cryptography.SHA1]::Create()
    try {
        $hash = $sha1.ComputeHash($data)
    }
    finally {
        $sha1.Dispose()
    }

    $bytes = [byte[]]$hash[0..15]
    $bytes[6] = ($bytes[6] -band 0x0F) -bor 0x50
    $bytes[8] = ($bytes[8] -band 0x3F) -bor 0x80

    [Array]::Reverse($bytes, 0, 4)
    [Array]::Reverse($bytes, 4, 2)
    [Array]::Reverse($bytes, 6, 2)

    return [guid]::new($bytes)
}

function Get-ToolsComponentGuid {
    param([Parameter(Mandatory)] [string] $RelativePath)

    $key = $RelativePath.ToLowerInvariant()
    if ($ToolsComponentGuidOverrides.ContainsKey($key)) { return $ToolsComponentGuidOverrides[$key] }

    $installPath = ('ProgramFiles64Folder\Windows MIDI Services\Tools\' + $RelativePath).ToLowerInvariant()
    (New-NameBasedGuid -Namespace $ToolsComponentGuidNamespace -Name $installPath).ToString()
}

# ----------------------------------------------------------------------------------------------
# Signing
# ----------------------------------------------------------------------------------------------

# A no-op unless -Sign was passed. The installers sign themselves through
# src/installers/Directory.Build.targets, which keys off MIDI_SIGNING_METADATA.
function Invoke-SignPath {
    param([Parameter(Mandatory)] [string[]] $Path)

    if (-not $Sign) { return }

    $existing = @($Path | Where-Object { $_ -and (Test-Path $_) })
    if ($existing.Count -eq 0) { return }

    & $SignScript -Path $existing -MetadataFile $SigningMetadata
}

# ----------------------------------------------------------------------------------------------
# Checks that run before anything is built
# ----------------------------------------------------------------------------------------------

# mididiag reports which servicing gates are present so support can read it off a customer machine,
# but that section is compiled out of public builds. Nothing else notices when the list drifts, and
# gates are retired on a rolling basis, so a stale entry surfaces much later as a bare C1083 on a
# header nobody remembers deleting. Check it before any compiling starts.
function Test-ServicingGateListing {
    $gateFolder = Join-Path $ApiRoot 'Inc'
    $mididiagSource = Join-Path $ApiRoot 'user-tools\mididiag\mididiag_main.cpp'

    if (-not (Test-Path $mididiagSource)) {
        throw "mididiag source not found: $mididiagSource"
    }

    $declared = @(Get-ChildItem (Join-Path $gateFolder 'Feature_Servicing_*.h') -ErrorAction SilentlyContinue |
        ForEach-Object { $_.BaseName -replace '^Feature_Servicing_', '' })

    if ($declared.Count -eq 0) {
        throw "No Feature_Servicing_*.h headers found under $gateFolder"
    }

    $source = Get-Content $mididiagSource -Raw

    $included = @([regex]::Matches($source, '#include\s+"Feature_Servicing_(\w+)\.h"') |
        ForEach-Object { $_.Groups[1].Value })

    $reported = @([regex]::Matches($source, 'OutputSingleFeatureEnablement\(\s*Feature_Servicing_(\w+)::IsEnabled\(\)\s*,\s*L"([^" (]+)') |
        ForEach-Object { [pscustomobject]@{ Gate = $_.Groups[1].Value; Label = $_.Groups[2].Value } })

    $reportedGates = @($reported | ForEach-Object { $_.Gate })

    $problems = @()

    foreach ($gate in ($declared | Where-Object { $reportedGates -notcontains $_ })) {
        $problems += "  $gate - header exists but mididiag does not report it"
    }

    foreach ($gate in ($reportedGates | Where-Object { $declared -notcontains $_ })) {
        $problems += "  $gate - mididiag reports it but the header is gone"
    }

    foreach ($gate in ($reportedGates | Where-Object { $included -notcontains $_ })) {
        $problems += "  $gate - reported by mididiag but never included"
    }

    # The listing is compiled out, so a copy-paste label naming the wrong gate is otherwise silent.
    foreach ($entry in ($reported | Where-Object { $_.Gate -ne $_.Label })) {
        $problems += "  $($entry.Gate) - reported under the wrong label '$($entry.Label)'"
    }

    if ($problems.Count -gt 0) {
        Write-Host ''
        Write-Host '     Servicing gate listing in mididiag is out of date:' -ForegroundColor Red
        $problems | Sort-Object -Unique | ForEach-Object { Write-Host $_ -ForegroundColor Red }
        Write-Host ''
        throw 'mididiag does not match the Feature_Servicing_*.h headers. Update DoSectionFeatureEnablement in mididiag_main.cpp.'
    }

    Write-Detail "Servicing gates $($declared.Count), all reported by mididiag"
}

# The apps share one install folder, so they share one copy of each runtime file they bring with
# them, such as the Windows App SDK bootstrapper. That only works if every app references the same
# version of each package. A drift here would otherwise show up at Stage as two different copies of
# a file, or not at all if the file names changed.
function Test-AppReferenceVersion {
    $projects = @(Get-ChildItem $UserToolsRoot -Recurse -Filter '*.vcxproj' |
        Where-Object { $_.FullName -notmatch '\\(bin|obj|packages)\\' })

    $references = foreach ($project in $projects) {
        $text = Get-Content $project.FullName -Raw
        foreach ($match in [regex]::Matches($text, '<PackageReference\s+Include="([^"]+)"\s+Version="([^"]+)"')) {
            [pscustomobject]@{ Package = $match.Groups[1].Value; Version = $match.Groups[2].Value; Project = $project.BaseName }
        }

        # The console tools still use packages.config.
        $packagesConfig = Join-Path $project.DirectoryName 'packages.config'
        if (Test-Path $packagesConfig) {
            foreach ($match in [regex]::Matches((Get-Content $packagesConfig -Raw), '<package\s+id="([^"]+)"\s+version="([^"]+)"')) {
                [pscustomobject]@{ Package = $match.Groups[1].Value; Version = $match.Groups[2].Value; Project = $project.BaseName }
            }
        }
    }

    $problems = @()

    foreach ($group in @($references | Group-Object Package)) {
        $versions = @($group.Group | Group-Object Version)
        if ($versions.Count -gt 1) {
            $problems += "  $($group.Name):"
            foreach ($version in $versions) {
                $problems += "    $($version.Name) in $(($version.Group | ForEach-Object { $_.Project } | Sort-Object) -join ', ')"
            }
        }
    }

    if ($problems.Count -gt 0) {
        Write-Host ''
        Write-Host '     The apps reference different versions of the same package:' -ForegroundColor Red
        $problems | ForEach-Object { Write-Host $_ -ForegroundColor Red }
        Write-Host ''
        throw 'Every app installs into the same folder, so every app must reference the same version of each package.'
    }

    Write-Detail "App references  $(@($references | Group-Object Package).Count) packages, one version each across $($projects.Count) projects"
}

# ----------------------------------------------------------------------------------------------
# Version
# ----------------------------------------------------------------------------------------------

function Get-TrainVersion {
    param([Parameter(Mandatory)] [string] $File)

    if (-not (Test-Path $File)) {
        throw "Version file not found: $File"
    }

    $json = Get-Content $File -Raw | ConvertFrom-Json

    if ($BumpBuildNumber) {
        $json.build = [int]$json.build + 1
        # Preserve the comment block and key order by round-tripping the whole object.
        $json | ConvertTo-Json -Depth 8 | Set-Content $File -Encoding UTF8
        Write-Detail "Bumped build number to $($json.build) in $(Split-Path -Leaf $File)"
    }

    $effectiveBuild = if ($BuildNumber -ge 0) { $BuildNumber } else { [int]$json.build }

    $majorMinorPatch = '{0}.{1}.{2}' -f $json.major, $json.minor, $json.patch

    # SemVer 2, e.g. 0.99.57-devpreview.5. The build number is deliberately absent: 'patch' is what
    # distinguishes releases. Stable drops the prerelease tag entirely.
    if ($json.channel -eq 'stable') {
        $semVer = $majorMinorPatch
    }
    else {
        $semVer = '{0}-{1}.{2}' -f $majorMinorPatch, $json.channel, $json.channelNumber
    }

    # Sample zips are named after the release rather than the version, matching the ones already
    # published: "SDK Dev Preview 7" -> "dev-preview-7". The leading "sdk" is dropped because the
    # zip names already say what they contain.
    $releaseName = (([string]$json.versionName).ToLowerInvariant() -replace '[^a-z0-9]+', '-').Trim('-') -replace '^sdk-', ''

    $packageId = if ($json.PSObject.Properties.Name -contains 'nuGetPackageId') { [string]$json.nuGetPackageId } else { $null }

    [pscustomobject]@{
        Major           = [int]$json.major
        Minor           = [int]$json.minor
        Patch           = [int]$json.patch
        Build           = $effectiveBuild
        Channel         = [string]$json.channel
        ChannelNumber   = [int]$json.channelNumber
        VersionName     = [string]$json.versionName
        ReleaseName     = $releaseName
        NuGetPackageId  = $packageId
        MajorMinorPatch = $majorMinorPatch
        SemVer          = $semVer
        # Four-part numeric, for assemblies, file versions and MSI packages.
        NumericVersion  = '{0}.{1}' -f $majorMinorPatch, $effectiveBuild
        # Folder-safe label for the release folder.
        ReleaseLabel    = ($semVer -replace '[^\w\.\-]', '-')
    }
}

function Write-VersionFile {
    param([Parameter(Mandatory)] [string] $Path, [Parameter(Mandatory)] [string] $Content)

    if (Set-ContentIfChanged -Path $Path -Value $Content) {
        Write-Detail "Wrote     $(Split-Path -Leaf $Path)"
    }
    else {
        Write-Detail "Unchanged $(Split-Path -Leaf $Path)"
    }
}

function Invoke-VersionTarget {
    Write-Step 'Version'

    $sdk = $script:Versions.Sdk
    $plugins = $script:Versions.Plugins
    $glass = $script:Versions.Glass

    Write-Detail "SDK and Tools   $($sdk.VersionName), $($sdk.SemVer), MSI $($sdk.NumericVersion)"
    Write-Detail "Transports      $($plugins.VersionName), $($plugins.SemVer), MSI $($plugins.NumericVersion)"
    Write-Detail "MIDI Glass      $($glass.VersionName), $($glass.SemVer), MSI $($glass.NumericVersion)"

    # --- SDK and Tools installer ---------------------------------------------------------------
    Write-VersionFile -Path $SdkVersionInclude -Content @"
<?xml version="1.0" encoding="utf-8"?>
<!-- Generated by build\build-all.ps1 from build\version.json. Do not edit. -->
<Include>
  <?define SetupVersionName="$($sdk.VersionName)" ?>
  <?define SetupVersionNumber="$($sdk.NumericVersion)" ?>
  <?define MidiSdkAndToolsVersion="$($sdk.NumericVersion)" ?>
  <?define MidiSdkAndToolsSemVer="$($sdk.SemVer)" ?>
  <?define StartMenuFolderName="$StartMenuFolderName" ?>
</Include>
"@

    # --- Transport installers ------------------------------------------------------------------
    # SetupVersionNumber is the Burn bundle version. Burn compares it as a semantic version, so the
    # prerelease tag is kept here rather than using the numeric form.
    Write-VersionFile -Path $PluginsVersionInclude -Content @"
<?xml version="1.0" encoding="utf-8"?>
<!-- Generated by build\build-all.ps1 from build\version-plugins.json. Do not edit. -->
<Include>
  <?define SetupVersionName="$($plugins.VersionName)" ?>
  <?define SetupVersionNumber="$($plugins.SemVer)" ?>
  <?define MidiSdkAndToolsVersion="$($plugins.SemVer)" ?>
  <?define MidiPluginsNumericVersion="$($plugins.NumericVersion)" ?>
</Include>
"@

    # --- MIDI Glass ------------------------------------------------------------------------------
    Write-VersionFile -Path $GlassVersionInclude -Content @"
<?xml version="1.0" encoding="utf-8"?>
<!-- Generated by build\build-all.ps1 from build\version-midi-glass.json. Do not edit. -->
<Include>
  <?define MidiGlassVersionName="$($glass.VersionName)" ?>
  <?define MidiGlassSetupVersion="$($glass.SemVer)" ?>
  <?define MidiGlassNumericVersion="$($glass.NumericVersion)" ?>
</Include>
"@

    # --- Version headers ---------------------------------------------------------------------------
    # midi2monitor's settings dialog shows *_BUILD_VERSION_FULL / *_BUILD_SOURCE; mididiag
    # includes the headers too. They carry the SDK version.
    $isPreview = if ($sdk.Channel -eq 'stable') { 'false' } else { 'true' }
    $previewTag = if ($sdk.Channel -eq 'stable') { '' } else { "$($sdk.Channel).$($sdk.ChannelNumber)" }

    foreach ($header in @(
            @{ File = 'WindowsMidiServicesVersion.h'; Prefix = 'WINDOWS_MIDI_SERVICES_NUGET' }
            @{ File = 'WindowsMidiServicesSdkRuntimeVersion.h'; Prefix = 'WINDOWS_MIDI_SERVICES_SDK_RUNTIME' }
        )) {

        $p = $header.Prefix
        Write-VersionFile -Path (Join-Path $VersionStagingFolder $header.File) -Content @"
// This file is generated by build\build-all.ps1 from build\version.json. Do not edit.
// The version information here represents the Windows MIDI Services App SDK version
// this binary was built against.

#ifndef $($p)_VERSION_INCLUDE
#define $($p)_VERSION_INCLUDE

#define $($p)_BUILD_IS_PREVIEW                         $isPreview
#define $($p)_BUILD_SOURCE                             L"$SdkBuildSource"
#define $($p)_BUILD_DATE                               L"$(Get-Date -Format 'yyyy-MM-dd')"
#define $($p)_BUILD_VERSION_NAME                       L"$($sdk.VersionName)"
#define $($p)_BUILD_VERSION_FULL                       L"$($sdk.SemVer)"
#define $($p)_BUILD_VERSION_MAJOR                      $($sdk.Major)
#define $($p)_BUILD_VERSION_MINOR                      $($sdk.Minor)
#define $($p)_BUILD_VERSION_PATCH                      $($sdk.Patch)
#define $($p)_BUILD_VERSION_BUILD_NUMBER               $($sdk.Build)
#define $($p)_BUILD_PREVIEW                            L"$previewTag"
#define $($p)_BUILD_VERSION_FILE                       L"$($sdk.NumericVersion)"

#endif
"@
    }

    # --- Stamp the nuspec version --------------------------------------------------------------
    # The nuspec is checked in with a literal <version>, so rewrite just that element.
    $nuspec = Get-Content $NuspecFile -Raw
    $updated = [regex]::Replace($nuspec, '<version>[^<]*</version>', "<version>$($sdk.SemVer)</version>", 1)

    if ($updated -ne $nuspec) {
        Set-Content -Path $NuspecFile -Value $updated -Encoding UTF8 -NoNewline
        Write-Detail "Stamped nuspec version -> $($sdk.SemVer)"
    }
    else {
        Write-Detail 'Nuspec version already current'
    }

    # Windows Installer only compares major.minor.build (our patch). Warn when a rebuild would not
    # upgrade over the previously installed package.
    Write-Note "MSI upgrade detection uses only major.minor.patch - bump 'patch' in the version file for any installer that must replace an installed one."
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

    # 64-bit MSBuild. The Arm64EC/Arm64X link steps and the WDK's INF verification need the amd64
    # host tools.
    $found = & $vswhere -latest -prerelease -products * `
        -requires Microsoft.Component.MSBuild `
        -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1

    if (-not $found) {
        throw 'Could not locate MSBuild.exe via vswhere. Pass -MSBuildPath explicitly.'
    }

    return $found
}

function Resolve-MakePri {
    $root = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    $arch = if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64') { 'arm64' } else { 'x64' }

    $found = @(Get-ChildItem $root -Directory -Filter '10.*' -ErrorAction SilentlyContinue |
        ForEach-Object {
            $exe = Join-Path $_.FullName "$arch\makepri.exe"
            if (Test-Path $exe) { [pscustomobject]@{ Version = [version]$_.Name; Path = $exe } }
        } | Sort-Object Version -Descending)

    if ($found.Count -eq 0) {
        throw "makepri.exe not found under $root. Install the Windows SDK."
    }

    return $found[0].Path
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

        # The 64-bit hosted compiler and linker. The default for a cross-compile is the 32-bit
        # hosted ones, which run out of address space during link-time code generation on the
        # larger projects here: C1002, "compiler is out of heap space in pass 2".
        '/p:PreferredToolArchitecture=x64'

        "/v:$Verbosity"
        '/nologo'
        '/nr:false'
    )

    # The app projects reach Windows.Devices.Midi2.vcxproj through more than one global-property
    # set, so MSBuild builds it twice in a single invocation. Both writes target the same
    # OutDir/IntDir, and in parallel they collide on the FileTracker logs (FTK1011).
    $msbuildArgs += if ($Serial) { '/m:1' } elseif ($MaxCpuCount -gt 0) { "/m:$MaxCpuCount" } else { '/m' }

    if ($Targets.Count -gt 0) { $msbuildArgs += "/t:$($Targets -join ';')" }

    # Several vcxproj files include headers relative to $(SolutionDir); building a project
    # directly without it fails with C1083 on MidiDefs.h.
    if ($SolutionDir) { $msbuildArgs += "/p:SolutionDir=$($SolutionDir.TrimEnd('\'))\" }

    foreach ($key in $Properties.Keys) { $msbuildArgs += "/p:$key=$($Properties[$key])" }

    Write-Detail "msbuild $(Split-Path -Leaf $ProjectOrSolution) [$Configuration|$BuildPlatform]$(if ($Targets.Count -gt 0) { " /t:$($Targets -join ';')" })"

    & $script:MSBuild @msbuildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed ($LASTEXITCODE): $ProjectOrSolution [$Configuration|$BuildPlatform]"
    }
}

function Get-VersionProperties {
    param($Version)
    @{
        'Version'              = $Version.SemVer
        'VersionPrefix'        = $Version.MajorMinorPatch
        'AssemblyVersion'      = $Version.NumericVersion
        'FileVersion'          = $Version.NumericVersion
        'InformationalVersion' = $Version.SemVer
    }
}

# ----------------------------------------------------------------------------------------------
# The SDK binaries that ship
# ----------------------------------------------------------------------------------------------

# The folder the NuGet package and every installer take Windows.Devices.Midi2 from. The Arm64X
# binary is linked by the Arm64EC configuration and lands in that configuration's output folder;
# the plain Arm64 build in the ARM64 folder is only what the Arm64 apps compile against.
function Get-SdkShippingFolder {
    param([Parameter(Mandatory)] [string] $BuildPlatform)

    $folderPlatform = if ($BuildPlatform -eq 'Arm64') { 'Arm64EC' } else { $BuildPlatform }
    Join-Path $SdkOutRoot "Windows.Devices.Midi2\$folderPlatform\$Configuration"
}

function Get-SdkShippingArchitecture {
    param([Parameter(Mandatory)] [string] $BuildPlatform)
    if ($BuildPlatform -eq 'Arm64') { 'Arm64X' } else { 'x64' }
}

function Get-NuGetPackagePath {
    $sdk = $script:Versions.Sdk
    Join-Path $SdkNuGetOutput "$($sdk.NuGetPackageId).$($sdk.SemVer).nupkg"
}

# Removing the linked binaries before the build is what proves the ones found afterwards came from
# this build. Nothing links against the import library, so this relinks the SDK and nothing else.
function Remove-SdkLinkOutput {
    param([Parameter(Mandatory)] [string] $BuildPlatform)

    $folder = Get-SdkShippingFolder $BuildPlatform

    foreach ($extension in @('dll', 'pdb', 'pri')) {
        $file = Join-Path $folder "Windows.Devices.Midi2.$extension"
        if (Test-Path $file) { Remove-Item $file -Force }
    }
}

function Test-SdkBuildOutput {
    param([Parameter(Mandatory)] [string] $BuildPlatform)

    $folder = Get-SdkShippingFolder $BuildPlatform

    foreach ($extension in @('dll', 'pri', 'pdb', 'winmd')) {
        $file = Join-Path $folder "Windows.Devices.Midi2.$extension"
        if (-not (Test-Path $file)) {
            throw "The $BuildPlatform SDK build did not produce $file"
        }
    }

    foreach ($extension in @('dll', 'pri', 'pdb')) {
        $file = Get-Item (Join-Path $folder "Windows.Devices.Midi2.$extension")
        if ($file.LastWriteTime -lt $script:PassStart) {
            throw "$($file.FullName) was not written by this build: it is from $($file.LastWriteTime), and this build started at $($script:PassStart)."
        }
    }

    $dll = Join-Path $folder 'Windows.Devices.Midi2.dll'
    $expected = Get-SdkShippingArchitecture $BuildPlatform
    $actual = Get-FileArchitecture $dll

    if ($actual -ne $expected) {
        throw "$dll is an $actual binary. The $BuildPlatform NuGet runtime folders and installers need the $expected build."
    }

    Write-Detail "SDK $BuildPlatform $expected, linked $((Get-Item $dll).LastWriteTime.ToString('HH:mm:ss')) by this build: $folder"
}

# The package is only ever made from binaries this build linked, checked entry by entry against
# the build output: architecture, and a byte for byte match.
function Test-NuGetPackage {
    param([Parameter(Mandatory)] [string] $Package)

    $x64 = Get-SdkShippingFolder 'x64'
    $arm64 = Get-SdkShippingFolder 'Arm64'

    $checks = @(
        @{ Entry = 'runtimes/win-x64/native/Windows.Devices.Midi2.dll';     Source = Join-Path $x64 'Windows.Devices.Midi2.dll';     Architecture = 'x64' }
        @{ Entry = 'runtimes/win-x64/native/Windows.Devices.Midi2.pri';     Source = Join-Path $x64 'Windows.Devices.Midi2.pri' }
        @{ Entry = 'runtimes/win-x64/native/Windows.Devices.Midi2.pdb';     Source = Join-Path $x64 'Windows.Devices.Midi2.pdb' }
        @{ Entry = 'runtimes/win-arm64/native/Windows.Devices.Midi2.dll';   Source = Join-Path $arm64 'Windows.Devices.Midi2.dll';   Architecture = 'Arm64X' }
        @{ Entry = 'runtimes/win-arm64/native/Windows.Devices.Midi2.pri';   Source = Join-Path $arm64 'Windows.Devices.Midi2.pri' }
        @{ Entry = 'runtimes/win-arm64/native/Windows.Devices.Midi2.pdb';   Source = Join-Path $arm64 'Windows.Devices.Midi2.pdb' }
        @{ Entry = 'runtimes/win-arm64ec/native/Windows.Devices.Midi2.dll'; Source = Join-Path $arm64 'Windows.Devices.Midi2.dll';   Architecture = 'Arm64X' }
        @{ Entry = 'runtimes/win-arm64ec/native/Windows.Devices.Midi2.pri'; Source = Join-Path $arm64 'Windows.Devices.Midi2.pri' }
        @{ Entry = 'runtimes/win-arm64ec/native/Windows.Devices.Midi2.pdb'; Source = Join-Path $arm64 'Windows.Devices.Midi2.pdb' }
        @{ Entry = 'ref/native/Windows.Devices.Midi2.winmd';                Source = Join-Path $x64 'Windows.Devices.Midi2.winmd' }
        @{ Entry = 'ref/net10.0/Windows.Devices.Midi2.winmd';               Source = Join-Path $x64 'Windows.Devices.Midi2.winmd' }
    )

    $zip = [System.IO.Compression.ZipFile]::OpenRead($Package)

    try {
        $entries = @{}
        foreach ($entry in $zip.Entries) { $entries[$entry.FullName.Replace('\', '/').ToLowerInvariant()] = $entry }

        foreach ($check in $checks) {
            $entry = $entries[$check.Entry.ToLowerInvariant()]
            if (-not $entry) {
                throw "$(Split-Path -Leaf $Package) has no $($check.Entry)"
            }

            $stream = $entry.Open()
            $buffer = [System.IO.MemoryStream]::new()
            try {
                $stream.CopyTo($buffer)
            }
            finally {
                $stream.Dispose()
            }

            $bytes = $buffer.ToArray()
            $buffer.Dispose()

            $sha = [System.Security.Cryptography.SHA256]::Create()
            try {
                $hash = [System.BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-', '')
            }
            finally {
                $sha.Dispose()
            }

            if ($hash -ne (Get-Sha256 $check.Source)) {
                throw "$($check.Entry) in $(Split-Path -Leaf $Package) is not the file this build produced: $($check.Source)"
            }

            if ($check.ContainsKey('Architecture')) {
                $actual = Get-PeArchitecture -Image $bytes
                if ($actual -ne $check.Architecture) {
                    throw "$($check.Entry) in $(Split-Path -Leaf $Package) is an $actual binary; it must be the $($check.Architecture) build."
                }
            }
        }
    }
    finally {
        $zip.Dispose()
    }

    Write-Detail "Checked $($checks.Count) package entries against this build:"
    Write-Detail '  runtimes\win-x64                      x64 build'
    Write-Detail '  runtimes\win-arm64, runtimes\win-arm64ec   Arm64X build'
    Write-Detail '  ref                                   x64 winmd'
}

# Compares every staged copy of the SDK with the one the package and the installers must share.
function Test-StagedSdkCopies {
    param(
        [Parameter(Mandatory)] [string] $BuildPlatform,
        # Fail rather than warn when the package differs. Right after packing, or before releasing,
        # a difference is a defect; on a later Stage it means the SDK was rebuilt since.
        [switch] $RequirePackageMatch
    )

    $source = Get-SdkShippingFolder $BuildPlatform
    $package = Get-NuGetPackagePath
    $packageFolder = if ($BuildPlatform -eq 'Arm64') { 'runtimes/win-arm64/native' } else { 'runtimes/win-x64/native' }

    $expected = @{}
    foreach ($name in @('Windows.Devices.Midi2.dll', 'Windows.Devices.Midi2.pri')) {
        $expected[$name] = Get-Sha256 (Join-Path $source $name)
    }

    $copies = @(Get-ChildItem $StagingRoot -Recurse -File -Include 'Windows.Devices.Midi2.dll', 'Windows.Devices.Midi2.pri' |
        Where-Object { $_.FullName -match "\\$([regex]::Escape($BuildPlatform))\\" -and $_.FullName -notmatch '\\samples\\' })

    foreach ($copy in $copies) {
        if ((Get-Sha256 $copy.FullName) -ne $expected[$copy.Name]) {
            throw "$($copy.FullName) is not the SDK build in $source"
        }
    }

    $packageNote = 'no NuGet package for this version to compare against'

    if (Test-Path $package) {
        $mismatch = @()
        $zip = [System.IO.Compression.ZipFile]::OpenRead($package)

        try {
            foreach ($name in $expected.Keys) {
                $entry = $zip.Entries | Where-Object { $_.FullName.Replace('\', '/') -ieq "$packageFolder/$name" } | Select-Object -First 1
                if (-not $entry) {
                    $mismatch += $name
                    continue
                }

                $stream = $entry.Open()
                $sha = [System.Security.Cryptography.SHA256]::Create()
                try {
                    $hash = [System.BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '')
                }
                finally {
                    $sha.Dispose()
                    $stream.Dispose()
                }

                if ($hash -ne $expected[$name]) { $mismatch += $name }
            }
        }
        finally {
            $zip.Dispose()
        }

        if ($mismatch.Count -gt 0) {
            $message = "The staged $BuildPlatform SDK ($($mismatch -join ', ')) is not what $(Split-Path -Leaf $package) carries."
            if ($RequirePackageMatch) { throw "$message Run Sdk, Pack and Stage again." }
            Write-Note "$message Run Sdk and Pack again before releasing."
            $packageNote = 'NOT the NuGet package build'
        }
        else {
            $packageNote = 'same as the NuGet package'
        }
    }

    Write-Detail "SDK $BuildPlatform $($copies.Count) staged copies, all the $(Get-SdkShippingArchitecture $BuildPlatform) build, $packageNote"
}

# ----------------------------------------------------------------------------------------------
# Build
# ----------------------------------------------------------------------------------------------

function Invoke-ServiceTarget {
    Write-Step 'Build service and transports (Midi2.sln)'

    foreach ($plat in $Platform) {
        Invoke-MSBuild -ProjectOrSolution $ServiceSolution -BuildPlatform $plat `
            -Properties @{ 'NoWarn' = 'MIDL2111' }
    }

    foreach ($plat in $Platform) {
        foreach ($binary in $Transports) {
            $dll = Join-Path $ServiceOutRoot "$plat\$Configuration\$binary.dll"
            if (-not (Test-Path $dll)) { throw "Midi2.sln did not produce $dll" }
        }
    }

    Write-Detail "Transports $($Transports.Count) for $($Platform -join ', ')"
}

function Update-ApiReferenceHeader {
    foreach ($plat in $ApiReferencePlatforms) {
        $destination = Join-Path $ApiReferenceRoot $plat
        foreach ($header in $ApiReferenceHeaders) {
            Copy-IfChanged -Source $header -DestinationFolder $destination
        }
    }

    Write-Detail "Shared API headers current for $($ApiReferencePlatforms -join ', ')"
}

function Invoke-SdkTarget {
    Write-Step 'Build SDK, apps and tools (Midi2-AppSDK.sln)'

    Update-ApiReferenceHeader

    $versionProps = Get-VersionProperties $script:Versions.Sdk
    # winmd/dll platform mismatch is expected for Arm64EC.
    $versionProps['NoWarn'] = 'MSB3271'
    # The package is made by the Pack target once every platform is built, never by one of these.
    $versionProps['GeneratePackageOnBuild'] = 'false'

    foreach ($plat in $Platform) {
        Remove-SdkLinkOutput -BuildPlatform $plat
    }

    # Arm64 first, then Arm64EC, which links the Arm64X binary out of the Arm64 and Arm64EC code.
    if ($Platform -contains 'Arm64') {
        Invoke-MSBuild -ProjectOrSolution $SdkSolution -BuildPlatform 'Arm64' -Properties $versionProps

        # Only the SDK and the projects it depends on. Solution-level target names do not work
        # here - MSBuild forwards the name to every project and they all fail with MSB4057.
        foreach ($project in $Arm64EcProjects) {
            Invoke-MSBuild -ProjectOrSolution (Join-Path $ApiRoot $project.Project) -BuildPlatform 'Arm64EC' `
                -Targets $project.Targets -Properties $versionProps -SolutionDir $ApiRoot
        }
    }

    if ($Platform -contains 'x64') {
        Invoke-MSBuild -ProjectOrSolution $SdkSolution -BuildPlatform 'x64' -Properties $versionProps
    }

    foreach ($plat in $Platform) {
        Test-SdkBuildOutput -BuildPlatform $plat
    }

    $script:SdkBuiltThisPass = @($Platform)
}

function Invoke-PackTarget {
    Write-Step 'Pack the NuGet package'

    foreach ($plat in @('x64', 'Arm64')) {
        if ($script:SdkBuiltThisPass -notcontains $plat) {
            throw "The package carries the x64 and Arm64X SDK builds, so both must come from this run. Run Pack together with Sdk for -Platform x64,Arm64."
        }
        Test-SdkBuildOutput -BuildPlatform $plat
    }

    # Exactly what the nuspec packages, signed where it sits, so the package and every installer
    # carry the same signed file.
    Invoke-SignPath -Path @(foreach ($plat in @('x64', 'Arm64')) { Join-Path (Get-SdkShippingFolder $plat) 'Windows.Devices.Midi2.dll' })

    $package = Get-NuGetPackagePath
    if (Test-Path $package) { Remove-Item $package -Force }

    $packProps = Get-VersionProperties $script:Versions.Sdk
    $packProps['NoBuild'] = 'true'

    Invoke-MSBuild -ProjectOrSolution $NuGetProject -BuildPlatform 'x64' -Targets @('Pack') `
        -Properties $packProps -SolutionDir $ApiRoot -Serial

    if (-not (Test-Path $package)) {
        throw "The NuGet package was not produced: $package"
    }

    Test-NuGetPackage -Package $package

    $script:PackedThisPass = $true
    Write-Detail "NuGet package: $package"
}

# ----------------------------------------------------------------------------------------------
# Samples
# ----------------------------------------------------------------------------------------------

function Set-FileTextPreservingBom {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Text
    )

    # Every release rewrites these files, so keeping the byte-for-byte encoding avoids a diff
    # that touches whole files instead of the version strings that actually changed.
    $head = [System.IO.File]::ReadAllBytes($Path) | Select-Object -First 3
    $hasBom = $head.Count -eq 3 -and $head[0] -eq 0xEF -and $head[1] -eq 0xBB -and $head[2] -eq 0xBF

    [System.IO.File]::WriteAllText($Path, $Text, [System.Text.UTF8Encoding]::new($hasBom))
}

function Get-SolutionProjectPath {
    param([Parameter(Mandatory)] [string] $SolutionPath)

    $folder = Split-Path -Parent $SolutionPath
    $declarations = [regex]::Matches(
        (Get-Content $SolutionPath -Raw),
        '^Project\("\{[^}]+\}"\)\s*=\s*"[^"]*",\s*"([^"]+)"',
        [System.Text.RegularExpressions.RegexOptions]::Multiline)

    $paths = foreach ($declaration in $declarations) {
        $relative = $declaration.Groups[1].Value

        # Solution folders are declared the same way but carry a name rather than a file path.
        if ($relative -notmatch '\.[a-z]*proj$') { continue }

        $full = Join-Path $folder $relative
        if (-not (Test-Path $full)) {
            throw "$(Split-Path -Leaf $SolutionPath) references a project that does not exist: $relative"
        }

        (Resolve-Path $full).Path
    }

    if (@($paths).Count -eq 0) {
        throw "No projects found in $SolutionPath"
    }

    , @($paths)
}

function Get-SolutionPlatform {
    param(
        [Parameter(Mandatory)] [string] $SolutionPath,
        [Parameter(Mandatory)] [string] $RequestedPlatform
    )

    # Solution platform names are spelled inconsistently across the samples (ARM64 vs Arm64), and
    # MSBuild will happily "succeed" while building nothing when the name does not resolve.
    $declared = [regex]::Matches(
        (Get-Content $SolutionPath -Raw),
        '^\s*' + [regex]::Escape($Configuration) + '\|(\S+) = ',
        [System.Text.RegularExpressions.RegexOptions]::Multiline)

    foreach ($match in $declared) {
        if ($match.Groups[1].Value -eq $RequestedPlatform) { return $match.Groups[1].Value }
    }

    return $null
}

function Update-SampleReference {
    param(
        [Parameter(Mandatory)] [string[]] $Projects,
        [Parameter(Mandatory)] $Version
    )

    $packageId = $Version.NuGetPackageId
    $updatedFiles = 0

    foreach ($project in $Projects) {
        $text = Get-Content $project -Raw
        $original = $text

        if ($project -like '*.vcxproj') {
            # packages.config projects hard-code the extracted package folder in four places: the
            # props import, the targets import and both EnsureNuGetPackageBuildImports checks.
            $text = [regex]::Replace($text,
                'packages\\' + [regex]::Escape($packageId) + '\.[^\\]+\\',
                'packages\' + $packageId + '.' + $Version.SemVer + '\')
        }
        else {
            $text = [regex]::Replace($text,
                '(<PackageReference\s+Include="' + [regex]::Escape($packageId) + '"\s+Version=")[^"]*(")',
                '${1}' + $Version.SemVer + '${2}')
        }

        if ($text -ne $original) {
            Set-FileTextPreservingBom -Path $project -Text $text
            $updatedFiles++
        }

        $packagesConfig = Join-Path (Split-Path -Parent $project) 'packages.config'
        if (Test-Path $packagesConfig) {
            $text = Get-Content $packagesConfig -Raw
            $original = $text

            $text = [regex]::Replace($text,
                '(<package\s+id="' + [regex]::Escape($packageId) + '"\s+version=")[^"]*(")',
                '${1}' + $Version.SemVer + '${2}')

            if ($text -ne $original) {
                Set-FileTextPreservingBom -Path $packagesConfig -Text $text
                $updatedFiles++
            }
        }
    }

    return $updatedFiles
}

function Test-SampleFileIncluded {
    param([Parameter(Mandatory)] [System.IO.FileInfo] $File)

    return -not ($SampleExcludedExtensions -contains $File.Extension)
}

function Copy-SampleTree {
    param(
        [Parameter(Mandatory)] [string] $Source,
        [Parameter(Mandatory)] [string] $Destination
    )

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null

    foreach ($file in @(Get-ChildItem $Source -File)) {
        if (Test-SampleFileIncluded -File $file) {
            Copy-Item $file.FullName -Destination (Join-Path $Destination $file.Name) -Force
        }
    }

    foreach ($folder in @(Get-ChildItem $Source -Directory)) {
        if ($SampleExcludedFolders -contains $folder.Name) { continue }
        Copy-SampleTree -Source $folder.FullName -Destination (Join-Path $Destination $folder.Name)
    }
}

function New-SampleArchive {
    param(
        [Parameter(Mandatory)] $Set,
        [Parameter(Mandatory)] [string[]] $Projects,
        [Parameter(Mandatory)] [string] $ArchivePath
    )

    $stagingFolder = Join-Path $StagingRoot "samples\$($Set.ZipPrefix)"
    New-EmptyFolder $stagingFolder

    # Solution, nuget.config and readme sit at the root; everything else comes from the folders
    # of the projects the solution actually declares, so retired samples left on disk stay out.
    foreach ($file in @(Get-ChildItem $Set.Folder -File)) {
        if (Test-SampleFileIncluded -File $file) {
            Copy-Item $file.FullName -Destination (Join-Path $stagingFolder $file.Name) -Force
        }
    }

    $projectFolders = $Projects | ForEach-Object { Split-Path -Parent $_ } | Sort-Object -Unique

    foreach ($source in $projectFolders) {
        $relative = $source.Substring($Set.Folder.Length).TrimStart('\')
        Copy-SampleTree -Source $source -Destination (Join-Path $stagingFolder $relative)
    }

    if (Test-Path $ArchivePath) { Remove-Item $ArchivePath -Force }

    # Not Compress-Archive: it writes backslash separators, which several extractors treat as
    # part of the file name rather than as a folder.
    [System.IO.Compression.ZipFile]::CreateFromDirectory(
        $stagingFolder, $ArchivePath, [System.IO.Compression.CompressionLevel]::Optimal, $false)

    return @(Get-ChildItem $stagingFolder -File -Recurse).Count
}

function Invoke-SamplesTarget {
    Write-Step 'Samples'

    $version = $script:Versions.Sdk

    $package = Get-NuGetPackagePath
    if (-not (Test-Path $package)) {
        throw "Cannot update the samples: the NuGet package has not been built. Run the Sdk and Pack targets first. ($package)"
    }

    # The samples reference the package by version from nuget.org, where this build has not been
    # published yet. RestoreAdditionalProjectSources adds the local output folder; RestoreSources
    # cannot be used because it replaces the configured feeds and MSBuild splits /p: values on ';'.
    $releaseFolder = Join-Path $ReleaseRoot $version.ReleaseLabel
    New-Item -ItemType Directory -Force -Path $releaseFolder | Out-Null

    foreach ($set in $SampleSets) {
        $solution = Join-Path $set.Folder $set.Solution
        if (-not (Test-Path $solution)) { throw "Sample solution not found: $solution" }

        Write-Detail "$($set.Name) samples"

        $projects = Get-SolutionProjectPath -SolutionPath $solution
        Write-Detail "  $($projects.Count) projects in $($set.Solution)"

        $updated = Update-SampleReference -Projects $projects -Version $version
        Write-Detail "  $updated files repointed at $($version.NuGetPackageId) $($version.SemVer)"

        foreach ($plat in $Platform) {
            $solutionPlatform = Get-SolutionPlatform -SolutionPath $solution -RequestedPlatform $plat
            if (-not $solutionPlatform) {
                Write-Note "  $($set.Solution) has no $Configuration|$plat configuration - skipped"
                continue
            }

            # RestorePackagesConfig covers the C++/WinRT samples, which are still packages.config.
            Invoke-MSBuild -ProjectOrSolution $solution -BuildPlatform $solutionPlatform `
                -Targets @('Restore') `
                -Properties @{ 'RestoreAdditionalProjectSources' = $SdkNuGetOutput; 'RestorePackagesConfig' = 'true' }

            Invoke-MSBuild -ProjectOrSolution $solution -BuildPlatform $solutionPlatform -Targets @('Build')
        }

        $archive = Join-Path $releaseFolder "$($set.ZipPrefix)-$($version.ReleaseName).zip"
        $fileCount = New-SampleArchive -Set $set -Projects $projects -ArchivePath $archive

        Write-Detail "  $fileCount files -> $(Split-Path -Leaf $archive)"
    }
}

# ----------------------------------------------------------------------------------------------
# Staging
# ----------------------------------------------------------------------------------------------

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

    New-EmptyFolder $Destination

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
            # The Sdk target already built the SDK and its projection. Building them again here
            # could relink the SDK after Pack, and it collides with any other build of the SDK.
            'BuildProjectReferences' = 'false'
            'GeneratePackageOnBuild' = 'false'
        }

    $produced = @(Get-ChildItem $Destination -File -ErrorAction SilentlyContinue)
    if ($produced.Count -eq 0) {
        throw "Publish reported success but produced no files in $Destination"
    }

    # @() matters: StrictMode turns $null.Count into an error when there are no subfolders.
    $subFolders = @(Get-ChildItem $Destination -Directory -ErrorAction SilentlyContinue)
    Write-Detail "  $($produced.Count) files, $($subFolders.Count) subfolders"
}

# An app's XAML is compiled into its own PRI, and that is what makes it safe to install apps whose
# XAML files share names (every one of them has App.xaml and MainWindow.xaml) into one folder: the
# loose .xbf files are never read, so they are not installed. This proves it for every page the
# build produced, so a change in how the build treats XAML fails here instead of on a customer's PC.
function Test-XamlCompiledIntoPri {
    param(
        [Parameter(Mandatory)] [string] $App,
        [Parameter(Mandatory)] [string] $OutputFolder,
        [Parameter(Mandatory)] [string] $BuildPlatform
    )

    $pri = Join-Path $OutputFolder "$App.pri"
    $pages = @(Get-ChildItem $OutputFolder -Recurse -File -Filter '*.xbf' |
        ForEach-Object { [System.IO.Path]::GetRelativePath($OutputFolder, $_.FullName).Replace('\', '/') })

    $dumpFolder = Join-Path $env:TEMP 'midi-build-all-pri'
    New-Item -ItemType Directory -Force -Path $dumpFolder | Out-Null
    $dump = Join-Path $dumpFolder "$BuildPlatform-$App.xml"

    & $script:MakePri dump /if $pri /of $dump /o /dt detailed | Out-Null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $dump)) {
        throw "makepri could not read $pri"
    }

    [xml] $document = Get-Content $dump -Raw

    $resources = @{}
    foreach ($resource in @($document.SelectNodes("//ResourceMapSubtree[@name='Files']//NamedResource"))) {
        $uri = [string]$resource.GetAttribute('uri')
        $marker = '/Files/'
        $at = $uri.IndexOf($marker, [System.StringComparison]::OrdinalIgnoreCase)
        if ($at -lt 0) { continue }

        $relative = $uri.Substring($at + $marker.Length)
        if ($relative -notlike '*.xbf') { continue }

        $kinds = @($resource.SelectNodes('Candidate') | ForEach-Object { $_.GetAttribute('type') })
        $resources[$relative.ToLowerInvariant()] = $kinds
    }

    $problems = @()

    foreach ($page in $pages) {
        $kinds = $resources[$page.ToLowerInvariant()]
        if (-not $kinds) {
            $problems += "$page is not in $App.pri"
        }
        elseif (@($kinds | Where-Object { $_ -ne 'EmbeddedData' }).Count -gt 0) {
            $problems += "$App.pri points at $page on disk instead of holding it"
        }
    }

    foreach ($relative in $resources.Keys) {
        if (@($resources[$relative] | Where-Object { $_ -ne 'EmbeddedData' }).Count -gt 0 -and $pages -notcontains $relative) {
            $problems += "$App.pri points at $relative on disk instead of holding it"
        }
    }

    if ($problems.Count -gt 0) {
        throw "The XAML for $App ($BuildPlatform) is not compiled into its PRI, so it cannot share the Tools folder with the other apps:`n  $($problems -join "`n  ")"
    }

    return $pages.Count
}

# Copies one app's build output into a staging folder for the Tools folder, leaving out what is not
# installed: build leftovers, the .xbf files compiled into the app's PRI, the SDK (which is added
# once, from the NuGet package build) and any resources.pri left behind by an older build.
function Copy-AppOutput {
    param(
        [Parameter(Mandatory)] [string] $App,
        [Parameter(Mandatory)] [string] $BuildPlatform,
        [Parameter(Mandatory)] [string] $Destination,
        # Relative path, lower case -> who staged it and its hash. Shared by every payload for the
        # Tools folder, because they all end up in the same folder on the customer's PC.
        [Parameter(Mandatory)] [AllowEmptyCollection()] [hashtable] $Owners,
        [Parameter(Mandatory)] [AllowEmptyCollection()] [System.Collections.Generic.List[string]] $Conflicts
    )

    $source = Join-Path $SdkOutRoot "$App\$BuildPlatform\$Configuration"

    if (-not (Test-Path (Join-Path $source "$App.exe"))) {
        throw "Build output not found for $App ($BuildPlatform): $source. Run the Sdk target first."
    }

    $isXamlApp = @(Get-ChildItem $source -Recurse -File -Filter '*.xbf').Count -gt 0

    $required = @("$App.exe")
    if ($isXamlApp) { $required += @("$App.pri", 'Microsoft.WindowsAppRuntime.Bootstrap.dll') }
    if ($AppRequiredFiles.ContainsKey($App)) { $required += $AppRequiredFiles[$App] }

    foreach ($file in $required) {
        if (-not (Test-Path (Join-Path $source $file))) {
            throw "$App ($BuildPlatform) is missing $file in $source"
        }
    }

    $pages = 0
    if ($isXamlApp) {
        $pages = Test-XamlCompiledIntoPri -App $App -OutputFolder $source -BuildPlatform $BuildPlatform
    }

    $files = @(Get-ChildItem $source -Recurse -File | Where-Object {
            $BuildOnlyExtensions -notcontains $_.Extension.ToLowerInvariant() -and
            $_.Extension -ne '.xbf' -and
            $_.Name -ne 'resources.pri' -and
            $_.Name -ne 'Windows.Devices.Midi2.dll' -and
            $_.Name -ne 'Windows.Devices.Midi2.pri'
        })

    foreach ($file in $files) {
        $relative = [System.IO.Path]::GetRelativePath($source, $file.FullName)
        $key = $relative.ToLowerInvariant()
        $hash = Get-Sha256 $file.FullName

        if ($Owners.ContainsKey($key)) {
            $owner = $Owners[$key]
            if ($owner.Hash -ne $hash) {
                $Conflicts.Add("$relative ($BuildPlatform): $($owner.App) and $App carry different files")
            }
            else {
                $owner['Carriers'] = $owner['Carriers'] + 1
            }
        }
        else {
            $Owners[$key] = @{ App = $App; Hash = $hash; Carriers = 1 }
        }

        $target = Join-Path $Destination $relative
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
        Copy-Item -Path $file.FullName -Destination $target -Force
    }

    return [pscustomobject]@{ Files = $files.Count; Pages = $pages }
}

function Invoke-StageTarget {
    Write-Step 'Stage'

    $script:MakePri = Resolve-MakePri
    $sdkVersion = $script:Versions.Sdk

    foreach ($plat in $Platform) {

        $rid = if ($plat -eq 'Arm64') { 'win-arm64' } else { 'win-x64' }

        # --- The SDK every app and the NuGet package share -------------------------------------
        $sdkSource = Get-SdkShippingFolder $plat
        $sdkDll = Join-Path $sdkSource 'Windows.Devices.Midi2.dll'

        if (-not (Test-Path $sdkDll)) {
            throw "SDK build output not found: $sdkDll. Run the Sdk target first."
        }

        $sdkArchitecture = Get-FileArchitecture $sdkDll
        if ($sdkArchitecture -ne (Get-SdkShippingArchitecture $plat)) {
            throw "$sdkDll is an $sdkArchitecture binary; the $plat installers need the $(Get-SdkShippingArchitecture $plat) build."
        }

        # Signed where the package takes it from, so the installers and the package carry the
        # same file. Already signed when Pack ran in this build.
        Invoke-SignPath -Path $sdkDll

        # --- Transports ------------------------------------------------------------------------
        $transportDestination = Join-Path $TransportStagingRoot $plat

        foreach ($binary in $Transports) {
            # No pdb: symbols are archived with the release rather than installed.
            Copy-Staged -Source (Join-Path $ServiceOutRoot "$plat\$Configuration\$binary.dll") -Destination $transportDestination
        }

        Write-Detail "$plat  $($Transports.Count) transports -> api\$plat"

        # --- Apps in the Tools folder ----------------------------------------------------------
        $owners = @{}
        $conflicts = [System.Collections.Generic.List[string]]::new()

        foreach ($payload in $ToolsFolderPayloads) {
            $destination = Join-Path $StagingRoot "$($payload.Name)\$plat"
            New-EmptyFolder $destination

            $pages = 0
            foreach ($app in $payload.Apps) {
                $result = Copy-AppOutput -App $app -BuildPlatform $plat -Destination $destination -Owners $owners -Conflicts $conflicts
                $pages += $result.Pages
            }

            foreach ($extension in @('dll', 'pri')) {
                Copy-Staged -Source (Join-Path $sdkSource "Windows.Devices.Midi2.$extension") -Destination $destination
            }

            $fileCount = @(Get-ChildItem $destination -Recurse -File).Count
            Write-Detail "$plat  $($payload.Name): $($payload.Apps.Count) apps, $fileCount files, $pages XAML pages checked in their PRIs"
        }

        if ($conflicts.Count -gt 0) {
            throw "These apps cannot share the Tools folder, because they carry different copies of the same file:`n  $($conflicts -join "`n  ")"
        }

        $shared = @($owners.Values | Where-Object { $_['Carriers'] -gt 1 }).Count
        Write-Detail "$plat  Tools folder: $($owners.Count + 2) files from all installers; $shared of them come from more than one app, identical in each, plus the SDK"

        # --- PowerShell module -----------------------------------------------------------------
        # Installed in a folder of its own, named after the module, but with the same SDK build.
        $psStaging = Join-Path $StagingRoot "midi-powershell\$plat"

        Publish-DotNetApp -Project $PowerShellProject -BuildPlatform $plat -RuntimeIdentifier $rid `
            -Destination $psStaging -Version $sdkVersion

        Move-StagedSymbols -Folder $psStaging -BuildPlatform $plat

        foreach ($extension in @('dll', 'pri')) {
            Copy-Staged -Source (Join-Path $sdkSource "Windows.Devices.Midi2.$extension") -Destination $psStaging
        }

        $manifest = Join-Path $psStaging 'WindowsMidiServices.psd1'
        Copy-Item (Join-Path (Split-Path -Parent $PowerShellProject) 'WindowsMidiServices.psd1') `
            -Destination $manifest -Force

        # ModuleVersion must parse as System.Version, so the SemVer prerelease tag cannot be used
        # here. Stamped on the staged copy so the source manifest stays untouched.
        $psd1 = Get-Content $manifest -Raw
        $psd1 = [regex]::Replace($psd1, "(?m)^(\s*ModuleVersion\s*=\s*)'[^']*'", "`${1}'$($sdkVersion.NumericVersion)'", 1)
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

    Write-Detail 'Staged transport and endpoint assets'

    # --- CollectMidiLogs ---------------------------------------------------------------------
    $logsStaging = Join-Path $StagingRoot 'CollectMidiLogs'
    foreach ($f in @('CollectMidiLogs.cmd', 'CollectMidiLogs.ps1', 'providers.wprp', 'tttraceall.psm1')) {
        Copy-Staged -Source (Join-Path $CollectMidiLogsRoot $f) -Destination $logsStaging
    }
    Write-Detail 'Staged CollectMidiLogs'

    # --- Sign ---------------------------------------------------------------------------------
    # Must happen before Setup: WiX embeds these files into the MSI packages, so signing them
    # afterwards would sign a copy nobody installs.
    if ($Sign) {
        $signPaths = @($logsStaging)
        foreach ($plat in $Platform) {
            $signPaths += @($Transports | ForEach-Object { Join-Path $TransportStagingRoot "$plat\$_.dll" })
            $signPaths += @($ToolsFolderPayloads | ForEach-Object { Join-Path $StagingRoot "$($_.Name)\$plat" })
            $signPaths += Join-Path $StagingRoot "midi-powershell\$plat"
        }

        Invoke-SignPath -Path $signPaths
    }

    # After signing, because signing is the last thing allowed to change a staged file.
    foreach ($plat in $Platform) {
        Test-StagedSdkCopies -BuildPlatform $plat -RequirePackageMatch:$script:PackedThisPass
    }
}

# ----------------------------------------------------------------------------------------------
# WiX file lists
#
# Walk what actually got staged and emit one ComponentGroup per package, instead of hand-listing
# files in the .wxs sources.
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
    # than concatenate.
    return $Prefix + [System.BitConverter]::ToString($bytes[0..7]).Replace('-', '')
}

# The first staged platform stands for all of them, so check the others staged the same files.
function Get-FileListPlatform {
    param([Parameter(Mandatory)] [string[]] $StagingFolders)

    $listPlatform = $Platform[0]

    foreach ($folder in $StagingFolders) {
        $reference = Join-Path $StagingRoot ($folder -replace '\$\(var\.Platform\)', $listPlatform)
        if (-not (Test-Path $reference)) {
            throw "Nothing staged in $reference. Run the Stage target first."
        }

        $referenceFiles = @(Get-ChildItem $reference -Recurse -File | ForEach-Object { [System.IO.Path]::GetRelativePath($reference, $_.FullName).ToLowerInvariant() } | Sort-Object)

        foreach ($plat in @($Platform | Select-Object -Skip 1)) {
            $other = Join-Path $StagingRoot ($folder -replace '\$\(var\.Platform\)', $plat)
            $otherFiles = @(Get-ChildItem $other -Recurse -File | ForEach-Object { [System.IO.Path]::GetRelativePath($other, $_.FullName).ToLowerInvariant() } | Sort-Object)

            $difference = @($referenceFiles | Where-Object { $otherFiles -notcontains $_ }) + @($otherFiles | Where-Object { $referenceFiles -notcontains $_ })
            if ($difference.Count -gt 0) {
                throw "$listPlatform and $plat staged different files in $folder, so one installer file list cannot describe both: $($difference -join ', ')"
            }
        }
    }

    return $listPlatform
}

function New-WixFileListFragment {
    param(
        [Parameter(Mandatory)] [string] $OutputFile,
        [Parameter(Mandatory)] [string] $ComponentGroupId,
        # Each entry: Staging (relative to build\staging, may contain $(var.Platform)),
        # DirectoryId, and ToolsFolder - true for the shared Tools folder, whose components get
        # GUIDs from their install path so every installer carrying a file owns the same one.
        [Parameter(Mandatory)] [object[]] $Folders,
        [hashtable] $FileIdOverrides = @{}
    )

    $listPlatform = Get-FileListPlatform -StagingFolders @($Folders | ForEach-Object { $_.Staging })

    $directories = [System.Text.StringBuilder]::new()
    $components = [System.Text.StringBuilder]::new()
    $fileCount = 0

    foreach ($folder in $Folders) {
        $probe = Join-Path $StagingRoot ($folder.Staging -replace '\$\(var\.Platform\)', $listPlatform)

        [void]$components.AppendLine('')
        [void]$components.AppendLine("      <!-- $($folder.Staging) -->")

        $subDirectoryIds = @{ '' = $folder.DirectoryId }

        foreach ($sub in @(Get-ChildItem $probe -Recurse -Directory | Sort-Object FullName)) {
            if (@(Get-ChildItem $sub.FullName -Recurse -File).Count -eq 0) { continue }

            $relative = [System.IO.Path]::GetRelativePath($probe, $sub.FullName)
            $parentRelative = Split-Path -Parent $relative
            $id = ($folder.DirectoryId + '_' + ($relative -replace '[^A-Za-z0-9]', '_'))
            $subDirectoryIds[$relative] = $id

            [void]$directories.AppendLine("    <DirectoryRef Id=`"$($subDirectoryIds[$parentRelative])`">")
            [void]$directories.AppendLine("      <Directory Id=`"$id`" Name=`"$($sub.Name)`" />")
            [void]$directories.AppendLine('    </DirectoryRef>')
        }

        foreach ($file in @(Get-ChildItem $probe -Recurse -File | Sort-Object FullName)) {
            $fileCount++

            $relative = [System.IO.Path]::GetRelativePath($probe, $file.FullName)
            $parentRelative = Split-Path -Parent $relative
            $directoryId = $subDirectoryIds[$parentRelative]

            $source = '$(StagingSourceRootFolder)\' + $folder.Staging + '\' + $relative
            $key = "$($folder.DirectoryId)|$relative".ToLowerInvariant()

            $componentId = Get-WixStableId -Prefix 'c' -Value $key
            $fileId = if (-not $parentRelative -and $FileIdOverrides.ContainsKey($file.Name)) {
                $FileIdOverrides[$file.Name]
            }
            else {
                Get-WixStableId -Prefix 'f' -Value $key
            }

            $guid = if ($folder.ToolsFolder) { " Guid=`"$(Get-ToolsComponentGuid -RelativePath $relative)`"" } else { '' }

            # One file per component: that is the MSI guidance, and it is what lets one installer
            # share a file with another without sharing anything else.
            [void]$components.AppendLine("      <Component Id=`"$componentId`"$guid Bitness=`"always64`" Directory=`"$directoryId`"> <!-- $relative -->")
            [void]$components.AppendLine("        <File Id=`"$fileId`" Source=`"$source`" Vital=`"true`" />")
            [void]$components.AppendLine('      </Component>')
        }
    }

    $sb = [System.Text.StringBuilder]::new()
    [void]$sb.AppendLine('<?xml version="1.0" encoding="utf-8"?>')
    [void]$sb.AppendLine('<!-- Generated by build\build-all.ps1 from build\staging. Do not edit; your changes will be overwritten. -->')
    [void]$sb.AppendLine('<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">')
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('  <?define StagingSourceRootFolder=$(env.MIDI_REPO_ROOT)\build\staging ?>')
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('  <Fragment>')
    if ($directories.Length -gt 0) {
        [void]$sb.Append($directories.ToString())
        [void]$sb.AppendLine('')
    }
    [void]$sb.AppendLine("    <ComponentGroup Id=`"$ComponentGroupId`">")
    [void]$sb.Append($components.ToString())
    [void]$sb.AppendLine('    </ComponentGroup>')
    [void]$sb.AppendLine('  </Fragment>')
    [void]$sb.Append('</Wix>')

    [void](Set-ContentIfChanged -Path $OutputFile -Value $sb.ToString())
    Write-Detail "Generated $([System.IO.Path]::GetRelativePath($InstallersRoot, $OutputFile)) ($fileCount files)"
}

function New-StartMenuFragment {
    param([Parameter(Mandatory)] [string] $OutputFile)

    $sb = [System.Text.StringBuilder]::new()
    [void]$sb.AppendLine('<?xml version="1.0" encoding="utf-8"?>')
    [void]$sb.AppendLine('<!-- Generated by build\build-all.ps1. Do not edit; your changes will be overwritten. -->')
    [void]$sb.AppendLine('<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">')
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('  <Fragment>')
    [void]$sb.AppendLine('    <StandardDirectory Id="ProgramMenuFolder">')
    [void]$sb.AppendLine("      <Directory Id=`"MIDI_PROGRAMS_FOLDER`" Name=`"$StartMenuFolderName`" />")
    [void]$sb.AppendLine('    </StandardDirectory>')
    [void]$sb.AppendLine('')

    $autostartTools = @($GuiTools | Where-Object { $_.PSObject.Properties.Name -contains 'RunAtLogon' -and $_.RunAtLogon })

    # !! THE INSTALLER DELIBERATELY DOES NOT TURN AUTOSTART ON. !!
    # Nothing should sit in the notification area of a PC whose owner has not asked for it, so a
    # clean install writes no Run entry and the app never starts. The searches below only find an
    # entry which is ALREADY there, and the component that writes it is conditioned on that, which
    # is what keeps an upgrade from undoing a customer who did ask for it: same component guid and
    # same key path as the previous build, so the value is reference counted across the upgrade
    # rather than removed. MIDI Settings owns this setting - see midi-settings\NotificationSettings.cpp.
    # A Property is not allowed inside a ComponentGroup, so these sit above it in the fragment.
    foreach ($tool in $autostartTools) {
        [void]$sb.AppendLine("    <Property Id=`"$($tool.Name.ToUpperInvariant())AUTOSTARTPRESENT`">")
        [void]$sb.AppendLine("      <RegistrySearch Id=`"$($tool.Name)AutostartSearch`"")
        [void]$sb.AppendLine('                      Root="HKLM"')
        [void]$sb.AppendLine('                      Key="SOFTWARE\Microsoft\Windows\CurrentVersion\Run"')
        [void]$sb.AppendLine('                      Name="WindowsMidiServicesNotifications"')
        [void]$sb.AppendLine('                      Type="raw"')
        [void]$sb.AppendLine('                      Bitness="always64" />')
        [void]$sb.AppendLine('    </Property>')
        [void]$sb.AppendLine('')
    }

    [void]$sb.AppendLine('    <ComponentGroup Id="ToolAppShortcuts">')
    [void]$sb.AppendLine('      <Component Id="ToolAppShortcutsComponent" Bitness="always64" Directory="MIDI_PROGRAMS_FOLDER" Guid="0d1b7b1e-3a5e-4a2f-9a3c-6f2b6c4d5e71">')

    foreach ($tool in $GuiTools) {
        [void]$sb.AppendLine("        <Shortcut Id=`"Shortcut_$($tool.Name)`"")
        [void]$sb.AppendLine("                  Name=`"$($tool.Display)`"")
        [void]$sb.AppendLine("                  Target=`"[#$($tool.Name)Exe]`"")

        # Most tools declare neither of the optional fields below, and Set-StrictMode makes a
        # missing property an error rather than $null, so presence is tested before value.
        if ($tool.PSObject.Properties.Name -contains 'Aumid' -and $tool.Aumid) {
            [void]$sb.AppendLine("                  WorkingDirectory=`"$ToolsDirectoryId`">")
            [void]$sb.AppendLine("          <ShortcutProperty Key=`"System.AppUserModel.ID`" Value=`"$($tool.Aumid)`" />")
            [void]$sb.AppendLine('        </Shortcut>')
        }
        else {
            [void]$sb.AppendLine("                  WorkingDirectory=`"$ToolsDirectoryId`" />")
        }
    }

    [void]$sb.AppendLine('        <RemoveFolder Id="RemoveMidiProgramsFolder_Tools" Directory="MIDI_PROGRAMS_FOLDER" On="uninstall" />')
    [void]$sb.AppendLine('        <RegistryKey Root="HKLM" Key="SOFTWARE\Microsoft\Windows MIDI Services\Desktop App SDK Runtime">')
    [void]$sb.AppendLine('          <RegistryValue Type="string" Name="ToolAppShortcuts" Value="installed" KeyPath="yes" />')
    [void]$sb.AppendLine('        </RegistryKey>')
    [void]$sb.AppendLine('      </Component>')

    foreach ($tool in $autostartTools) {
        # Separate component, and the Run value is deliberately not the key path. MIDI Settings
        # lets a customer turn this off by deleting the value, and an MSI repair would put back
        # anything it holds the key path for.
        [void]$sb.AppendLine("      <Component Id=`"$($tool.Name)Autostart`" Bitness=`"always64`" Directory=`"MIDI_PROGRAMS_FOLDER`" Guid=`"6f3a9c21-58d4-4b7e-b1a6-0c9d3e7f2a48`" Condition=`"$($tool.Name.ToUpperInvariant())AUTOSTARTPRESENT`">")
        [void]$sb.AppendLine('        <RegistryKey Root="HKLM" Key="SOFTWARE\Microsoft\Windows\CurrentVersion\Run">')
        # Quoted: the install path contains a space, and Run splits an unquoted value on it.
        [void]$sb.AppendLine("          <RegistryValue Type=`"string`" Name=`"WindowsMidiServicesNotifications`" Value=`"&quot;[#$($tool.Name)Exe]&quot;`" />")
        [void]$sb.AppendLine('        </RegistryKey>')
        [void]$sb.AppendLine('        <RegistryKey Root="HKLM" Key="SOFTWARE\Microsoft\Windows MIDI Services\Desktop App SDK Runtime">')
        [void]$sb.AppendLine("          <RegistryValue Type=`"string`" Name=`"$($tool.Name)Autostart`" Value=`"installed`" KeyPath=`"yes`" />")
        [void]$sb.AppendLine('        </RegistryKey>')
        [void]$sb.AppendLine('      </Component>')
    }
    [void]$sb.AppendLine('    </ComponentGroup>')
    [void]$sb.AppendLine('  </Fragment>')
    [void]$sb.Append('</Wix>')

    [void](Set-ContentIfChanged -Path $OutputFile -Value $sb.ToString())
    Write-Detail "Generated $([System.IO.Path]::GetRelativePath($InstallersRoot, $OutputFile)) ($($GuiTools.Count) shortcuts)"
}

function New-SetupFileLists {
    # Fixed ids for the executables, so shortcuts, file associations and custom actions can refer
    # to them with [#Id].
    $exeIds = @{ 'mididiag.exe' = 'MidiDiagExe'; 'midi.exe' = 'MidiConsoleExe' }
    foreach ($payload in $ToolsFolderPayloads) {
        foreach ($app in $payload.Apps) {
            if (-not $exeIds.ContainsKey("$app.exe")) { $exeIds["$app.exe"] = "$($app)Exe" }
        }
    }

    foreach ($payload in $ToolsFolderPayloads) {
        $folders = @([pscustomobject]@{ Staging = "$($payload.Name)\`$(var.Platform)"; DirectoryId = $ToolsDirectoryId; ToolsFolder = $true })

        # Only the Tools installer carries these, and they do not go in the Tools folder. The
        # shared endpoint and transport art lives under ProgramData rather than Program Files,
        # because every MIDI app reads it and the customer's own pictures land beside it.
        if ($payload.Name -eq 'app-sdk') {
            $folders += [pscustomobject]@{ Staging = 'CollectMidiLogs'; DirectoryId = 'COLLECTMIDILOGS_INSTALLFOLDER'; ToolsFolder = $false }
            $folders += [pscustomobject]@{ Staging = 'Assets\Endpoints'; DirectoryId = 'CONFIGURATION_ASSETS_ENDPOINTS_FOLDER'; ToolsFolder = $false }
            $folders += [pscustomobject]@{ Staging = 'Assets\Transports'; DirectoryId = 'CONFIGURATION_ASSETS_TRANSPORTS_FOLDER'; ToolsFolder = $false }
        }

        New-WixFileListFragment -OutputFile $payload.Fragment -ComponentGroupId $payload.ComponentGroup `
            -Folders $folders -FileIdOverrides $exeIds
    }

    New-StartMenuFragment -OutputFile (Join-Path $InstallersRoot 'api-and-tools-installer\sdk-package\_StartMenu.wxs')

    New-WixFileListFragment `
        -OutputFile (Join-Path $InstallersRoot 'api-and-tools-installer\powershell-package\_SetupFiles.wxs') `
        -ComponentGroupId 'PowerShellModuleFiles' `
        -Folders @([pscustomobject]@{ Staging = 'midi-powershell\$(var.Platform)'; DirectoryId = 'POWERSHELL_MODULE_INSTALLFOLDER'; ToolsFolder = $false })
}

# ----------------------------------------------------------------------------------------------
# Setup
# ----------------------------------------------------------------------------------------------

function Get-InstallerPath {
    param([Parameter(Mandatory)] $Installer, [Parameter(Mandatory)] [string] $BuildPlatform)
    Join-Path $Installer.SolutionDir "main-bundle\bin\$BuildPlatform\$Configuration\$($Installer.BundleName).exe"
}

function Invoke-SetupTarget {
    Write-Step 'Setup'

    foreach ($include in @($SdkVersionInclude, $PluginsVersionInclude, $GlassVersionInclude)) {
        if (-not (Test-Path $include)) {
            throw "Version include not found: $include. Run the Version target first."
        }
    }

    New-SetupFileLists

    foreach ($installer in $Installers) {
        foreach ($plat in $Platform) {
            $solution = Join-Path $installer.SolutionDir $installer.Solution

            Invoke-MSBuild -ProjectOrSolution $solution -BuildPlatform $plat `
                -Targets @('Restore', 'Rebuild') -SolutionDir $installer.SolutionDir

            $bundle = Get-InstallerPath -Installer $installer -BuildPlatform $plat
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

function Get-InstallerSymbolSource {
    param([Parameter(Mandatory)] $Installer, [Parameter(Mandatory)] [string] $BuildPlatform)

    $sources = @()

    if ($Installer.Name -eq 'Tools') {
        $sources += Join-Path (Get-SdkShippingFolder $BuildPlatform) 'Windows.Devices.Midi2.pdb'

        # The managed packages publish into staging, so their symbols were set aside there rather
        # than being looked for under a per-project output path.
        $stagedSymbols = Join-Path $StagingRoot "symbols\$BuildPlatform"
        if (Test-Path $stagedSymbols) {
            $sources += @(Get-ChildItem $stagedSymbols -File -Filter '*.pdb' | Select-Object -ExpandProperty FullName)
        }
        else {
            Write-Note "No staged symbols for $BuildPlatform - run the Stage target"
        }
    }

    foreach ($app in $Installer.Apps) {
        $sources += Join-Path $SdkOutRoot "$app\$BuildPlatform\$Configuration\$app.pdb"
    }

    foreach ($binary in $Installer.Transports) {
        $sources += Join-Path $ServiceOutRoot "$BuildPlatform\$Configuration\$binary.pdb"
    }

    return , @($sources)
}

function Invoke-ReleaseTarget {
    Write-Step 'Release'

    # The package and the installers must carry the same SDK. Stage checks it too, but only warns
    # when the package came from an earlier run; nothing inconsistent goes into a release.
    $package = Get-NuGetPackagePath
    foreach ($plat in $Platform) {
        Test-StagedSdkCopies -BuildPlatform $plat -RequirePackageMatch
    }

    $folders = @{}

    foreach ($installer in $Installers) {
        $version = $script:Versions[$installer.Train]
        $folder = Join-Path $ReleaseRoot ($installer.ReleaseFolder -f $version.ReleaseLabel)
        New-Item -ItemType Directory -Force -Path $folder | Out-Null
        $folders[$folder] = $true

        foreach ($plat in $Platform) {
            $bundle = Get-InstallerPath -Installer $installer -BuildPlatform $plat
            if (-not (Test-Path $bundle)) {
                Write-Note "Installer not found for $($installer.Name) $plat - run the Setup target: $bundle"
                continue
            }

            $name = "$($installer.ReleaseName) $($version.SemVer)-$($plat.ToLowerInvariant()).exe"
            Copy-Item $bundle -Destination (Join-Path $folder $name) -Force
            Write-Detail "$([System.IO.Path]::GetRelativePath($ReleaseRoot, $folder))\$name"
        }

        # A crash report gives a module name, a build timestamp and an offset. Without the pdb from
        # that exact build the offset cannot be turned back into a function, and the build output
        # is overwritten by the next build. This is the only copy that survives, because nothing
        # here is installed on a customer's PC.
        foreach ($plat in $Platform) {
            $symbolFolder = Join-Path $folder "symbols\$plat"
            New-Item -ItemType Directory -Force -Path $symbolFolder | Out-Null

            foreach ($source in (Get-InstallerSymbolSource -Installer $installer -BuildPlatform $plat)) {
                if (Test-Path $source) {
                    Copy-Item $source -Destination $symbolFolder -Force
                }
                else {
                    Write-Note "Symbols not found: $source"
                }
            }
        }
    }

    $sdkFolder = Join-Path $ReleaseRoot $script:Versions.Sdk.ReleaseLabel
    if (Test-Path $package) {
        Copy-Item $package -Destination $sdkFolder -Force
        Write-Detail "$([System.IO.Path]::GetRelativePath($ReleaseRoot, $sdkFolder))\$(Split-Path -Leaf $package)"
    }
    else {
        Write-Note "NuGet package not found: $package"
    }

    Write-Host ''
    foreach ($folder in ($folders.Keys | Sort-Object)) {
        Write-Host "     Release folder: $folder" -ForegroundColor Green
    }
}

# ----------------------------------------------------------------------------------------------
# Clean
# ----------------------------------------------------------------------------------------------

function Invoke-CleanTarget {
    Write-Step 'Clean'

    # Only what the build writes. build\staging also holds a few files that are checked in.
    $paths = @(
        'app-sdk', 'midi-console', 'midi-powershell', 'midi-glass', 'network-app', 'bluetooth-app',
        'api', 'CollectMidiLogs', 'Assets', 'symbols', 'samples'
    ) | ForEach-Object { Join-Path $StagingRoot $_ }

    $paths += @($SdkOutRoot, $SdkIntermediateRoot, (Join-Path $ServiceOutRoot 'intermediate'))

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

# @() matters: a one-element array unrolls to a scalar through an if-expression, and StrictMode
# then fails the .Count test below.
$targets = @(if ($Target -contains 'All') { $AllTargets } else { $Target | Where-Object { $_ -ne 'Clean' } })
$explicit = @($Target)

# The package holds the x64 and Arm64X builds, so it cannot be made from one platform, and the
# samples compile against it.
$bothPlatforms = ($Platform -contains 'x64') -and ($Platform -contains 'Arm64')

if (-not $bothPlatforms) {
    foreach ($needsBoth in @('Pack', 'Samples')) {
        if ($targets -contains $needsBoth) {
            if ($explicit -contains $needsBoth) {
                throw "$needsBoth needs -Platform x64,Arm64: the NuGet package carries both builds."
            }
            $targets = @($targets | Where-Object { $_ -ne $needsBoth })
            Write-Note "$needsBoth skipped: the NuGet package needs both x64 and Arm64 in the same run."
        }
    }
}

if ($targets -contains 'Pack' -and $targets -notcontains 'Sdk') {
    throw 'Pack runs only together with Sdk, so the package can only ever hold binaries from the build it is part of.'
}

$parallelism = if ($MaxCpuCount -gt 0) { "$MaxCpuCount of $([Environment]::ProcessorCount)" } else { "all $([Environment]::ProcessorCount)" }

Write-Host ''
Write-Host 'Windows MIDI Services - SDK, tools, apps and transports' -ForegroundColor White
Write-Detail "Repo          $RepoRoot"
Write-Detail "Targets       $((@($(if ($Target -contains 'Clean') { 'Clean' })) + $targets | Where-Object { $_ }) -join ', ')"
Write-Detail "Platforms     $($Platform -join ', ')"
Write-Detail "Configuration $Configuration"
Write-Detail "Parallelism   $parallelism logical processors, $Priority priority"

if ($Sign) {
    if (-not $SigningMetadata) {
        throw '-Sign needs -SigningMetadata, or the MIDI_SIGNING_METADATA environment variable, pointing at the Artifact Signing metadata JSON. See build\sign-files.ps1.'
    }
    if (-not (Test-Path $SigningMetadata)) {
        throw "Signing metadata file not found: $SigningMetadata"
    }

    $SigningMetadata = (Resolve-Path $SigningMetadata).Path
    Write-Detail "Signing       $SigningMetadata"
}

# The WiX projects sign themselves off this variable (src/installers/Directory.Build.targets). It
# is set or cleared here rather than inherited, so a signed installer can never end up wrapped
# around an unsigned payload because the variable happened to be set in the shell.
$env:MIDI_SIGNING_METADATA = if ($Sign) { $SigningMetadata } else { '' }

if ($Target -contains 'Clean') {
    Invoke-CleanTarget
    if ($targets.Count -eq 0) { return }
}

# MSBuild and the compilers inherit this process's priority class, so setting it here is what
# keeps a build off the desktop's back. Restored below so an interactive shell is not left low.
$previousPriority = [System.Diagnostics.Process]::GetCurrentProcess().PriorityClass
[System.Diagnostics.Process]::GetCurrentProcess().PriorityClass = [System.Diagnostics.ProcessPriorityClass]$Priority

try {
    Test-ServicingGateListing
    Test-AppReferenceVersion

    $script:Versions = @{
        Sdk     = Get-TrainVersion -File $SdkVersionFile
        Plugins = Get-TrainVersion -File $PluginsVersionFile
        Glass   = Get-TrainVersion -File $GlassVersionFile
    }

    if ($targets -notcontains 'Version') {
        Write-Detail "Versions      SDK $($script:Versions.Sdk.SemVer), transports $($script:Versions.Plugins.SemVer), MIDI Glass $($script:Versions.Glass.SemVer)"
    }

    if (@($targets | Where-Object { $_ -in @('Service', 'Sdk', 'Pack', 'Samples', 'Stage', 'Setup') }).Count -gt 0) {
        $script:MSBuild = Resolve-MSBuild
        Write-Detail "MSBuild       $script:MSBuild"
    }

    # Anything the SDK build links before this moment is, by definition, not from this build.
    $script:PassStart = Get-Date
    $script:SdkBuiltThisPass = @()
    $script:PackedThisPass = $false

    if ($targets -contains 'Version') { Invoke-VersionTarget }
    if ($targets -contains 'Service') { Invoke-ServiceTarget }
    if ($targets -contains 'Sdk') { Invoke-SdkTarget }
    if ($targets -contains 'Pack') { Invoke-PackTarget }
    if ($targets -contains 'Samples') { Invoke-SamplesTarget }
    if ($targets -contains 'Stage') { Invoke-StageTarget }
    if ($targets -contains 'Setup') { Invoke-SetupTarget }
    if ($targets -contains 'Release') { Invoke-ReleaseTarget }
}
finally {
    [System.Diagnostics.Process]::GetCurrentProcess().PriorityClass = $previousPriority
}

$stopwatch.Stop()
Write-Host ''
Write-Host ("Done in {0:hh\:mm\:ss}" -f $stopwatch.Elapsed) -ForegroundColor Green
Write-Host ''

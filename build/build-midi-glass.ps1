<#
.SYNOPSIS
    Builds, stages and packages the MIDI Glass installer.

.DESCRIPTION
    MIDI Glass has an installer of its own rather than shipping inside the App SDK runtime
    installer. It is a larger app on its own release train, and a customer who wants MIDI Glass
    should not have to take a new SDK runtime to get a fix to it, or the other way round.

    Version numbers come from build/version-midi-glass.json and are written to
    build/staging/version/MidiGlassVersion.wxi, which nothing else reads.

    The app installs to the same place it always has, Program Files\Windows MIDI Services\Tools\Glass,
    because MIDI Settings finds the tools it launches by that path.

    The whole build output is staged, not a hand-kept list: MIDI Glass is an unpackaged WinUI app,
    so its .xbf files have to keep their folder layout (Themes\Generic.xbf holds the templates for
    the surface's own controls) along with resources.pri, the Win2D runtime and the Windows App
    Runtime bootstrapper. The installer's file list is generated from what was staged, so a new
    file or folder in the build output is picked up without editing the .wxs by hand.

    The bundle carries the Visual C++ runtime and the Windows App Runtime, the same as the other
    stand-alone installers, because a customer installing only MIDI Glass may not have the App SDK
    runtime installer that normally provides them.

.PARAMETER Target
    One or more of:
      Version  Compute versions and write MidiGlassVersion.wxi.
      App      Build midiglass.vcxproj for each platform, with everything it references.
      Stage    Copy the app into build/staging/midi-glass.
      Setup    Generate the installer's file list from the staged app, then build the installer.
      Release  Collect the installer into build/release/midi-glass-<version>.
      Clean    Delete the MIDI Glass staging folder.
      All      Version, App, Stage, Setup, Release.

.PARAMETER BuildNumber
    Overrides the 'build' field in version-midi-glass.json without modifying the file. For CI.

.PARAMETER BumpBuildNumber
    Increments and persists the 'build' field in version-midi-glass.json before computing versions.

.PARAMETER Sign
    Authenticode-sign the staged app, the MSI package and the installer bundle. Needs the Artifact
    Signing client tools and a signed-in identity; see build/sign-files.ps1.

.PARAMETER SigningMetadata
    The Artifact Signing metadata JSON used by -Sign. Defaults to MIDI_SIGNING_METADATA.

.PARAMETER MaxCpuCount
    How many projects MSBuild builds at once. Defaults to three quarters of the logical
    processors so the machine stays usable while a build runs. 0 uses every logical processor.

.PARAMETER Priority
    Process priority for MSBuild and the compilers, which inherit it from this script.
    BelowNormal (the default) keeps the UI responsive. Use Normal on a build machine.

.EXAMPLE
    .\build-midi-glass.ps1
    Full MIDI Glass build for x64 and Arm64.

.EXAMPLE
    .\build-midi-glass.ps1 -Target Stage,Setup -Platform x64
    Restage whatever was last built and rebuild just the x64 installer.
#>
[CmdletBinding()]
param(
    # Comma-separated is accepted as a single token so this works through `pwsh -File`
    # and build-midi-glass.cmd, which do not split array arguments.
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

$Target = Expand-Argument -Value $Target -Allowed @('All', 'Version', 'App', 'Stage', 'Setup', 'Release', 'Clean') -Name 'Target'
$Platform = Expand-Argument -Value $Platform -Allowed @('x64', 'Arm64') -Name 'Platform'

# ----------------------------------------------------------------------------------------------
# Paths
# ----------------------------------------------------------------------------------------------

$BuildRoot = $PSScriptRoot
$RepoRoot = Split-Path -Parent $BuildRoot

$SourceRoot = Join-Path $RepoRoot 'src'
$ApiRoot = Join-Path $SourceRoot 'in-box'

$AppProject = Join-Path $ApiRoot 'user-tools\midi-glass\midiglass.vcxproj'
$AppOutRoot = Join-Path $ApiRoot 'vsfiles-sdk\out\midiglass'

$StagingRoot = Join-Path $BuildRoot 'staging'
$ReleaseRoot = Join-Path $BuildRoot 'release'
$AppStagingRoot = Join-Path $StagingRoot 'midi-glass'
$VersionStagingFolder = Join-Path $StagingRoot 'version'

# Deliberately not BundleInfo.wxi or AppSdkVersion.wxi: those belong to the other release trains,
# and sharing one means whichever build ran last wins.
$VersionIncludeFile = Join-Path $VersionStagingFolder 'MidiGlassVersion.wxi'

$VersionFile = Join-Path $BuildRoot 'version-midi-glass.json'

$SignScript = Join-Path $BuildRoot 'sign-files.ps1'

$SetupSolutionDir = Join-Path $SourceRoot 'installers\midi-glass-installer'
$SetupSolution = Join-Path $SetupSolutionDir 'midi-glass-setup.sln'
$FileListFile = Join-Path $SetupSolutionDir 'app-package\_AppFiles.wxs'

$BundleName = 'WindowsMidiServicesMidiGlassSetup'
$InstallerName = 'MIDI Glass'

# The MSI Directory the app installs into. Must match app-package\MidiGlass.wxs, and the folder
# name under Tools is a contract with MIDI Settings.
$AppDirectoryId = 'TOOL_GLASS_FOLDER'

# Build-time only. The pdbs alone are over 200 MB, and nothing reads .winmd at run time: C++/WinRT
# resolves types at compile time and activation goes through the app manifest.
$BuildOnlyExtensions = @('.exp', '.lib', '.winmd', '.ipdb', '.iobj', '.pdb')

# The .wxs files resolve staging as "$(env.MIDI_REPO_ROOT)\build\staging", so this must NOT
# have a trailing separator.
$env:MIDI_REPO_ROOT = $RepoRoot.TrimEnd('\')

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
# Signing
# ----------------------------------------------------------------------------------------------

# A no-op unless -Sign was passed. The installer signs itself through
# src/installers/Directory.Build.targets, which keys off MIDI_SIGNING_METADATA.
function Invoke-SignPath {
    param([Parameter(Mandatory)] [string[]] $Path)

    if (-not $Sign) { return }

    $existing = @($Path | Where-Object { $_ -and (Test-Path $_) })
    if ($existing.Count -eq 0) { return }

    & $SignScript -Path $existing -MetadataFile $SigningMetadata
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
        Write-Detail "Bumped build number to $($json.build) in version-midi-glass.json"
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

    # MidiGlassSetupVersion is the Burn bundle version. Burn compares it as a semantic version, so
    # the prerelease tag is kept here rather than using the numeric form.
    $include = @"
<?xml version="1.0" encoding="utf-8"?>
<!-- Generated from build\version-midi-glass.json by build-midi-glass.ps1. Do not edit. -->
<Include>
  <?define MidiGlassVersionName="$($Version.VersionName)" ?>
  <?define MidiGlassSetupVersion="$($Version.SemVer)" ?>
  <?define MidiGlassNumericVersion="$($Version.NumericVersion)" ?>
</Include>
"@
    Set-Content -Path $VersionIncludeFile -Value $include -Encoding UTF8
    Write-Detail "Wrote $VersionIncludeFile"
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
        [string] $SolutionDir
    )

    $msbuildArgs = @(
        $ProjectOrSolution
        "/p:Configuration=$Configuration"
        "/p:Platform=$BuildPlatform"

        # The 64-bit hosted compiler and linker. The 32-bit hosted ones run out of address space
        # during link-time code generation on the larger projects here.
        '/p:PreferredToolArchitecture=x64'

        "/v:$Verbosity"
        '/nologo'
        '/nr:false'
    )

    $msbuildArgs += if ($MaxCpuCount -gt 0) { "/m:$MaxCpuCount" } else { '/m' }

    if ($Targets.Count -gt 0) { $msbuildArgs += "/t:$($Targets -join ';')" }

    # The app and the projects it references include headers relative to $(SolutionDir).
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

function Invoke-AppTarget {
    Write-Step 'Build MIDI Glass'

    foreach ($plat in $Platform) {
        # The SDK NuGet package is not part of this installer, and its nuspec pulls from output
        # that only build-sdk.ps1 produces.
        Invoke-MSBuild -ProjectOrSolution $AppProject -BuildPlatform $plat -SolutionDir $ApiRoot `
            -Properties @{ 'NoWarn' = 'MIDL2111'; 'GeneratePackageOnBuild' = 'false' }
    }
}

# ----------------------------------------------------------------------------------------------
# Staging
# ----------------------------------------------------------------------------------------------

function Invoke-StageTarget {
    Write-Step 'Stage'

    foreach ($plat in $Platform) {
        $source = Join-Path $AppOutRoot "$plat\$Configuration"
        $destination = Join-Path $AppStagingRoot $plat

        if (-not (Test-Path (Join-Path $source 'midiglass.exe'))) {
            throw "MIDI Glass build output not found: $source. Run the App target first."
        }

        # A stale file here would be silently packaged, so the folder is emptied rather than
        # copied over.
        if (Test-Path $destination) { Remove-Item $destination -Recurse -Force }
        New-Item -ItemType Directory -Force -Path $destination | Out-Null

        Copy-Item -Path (Join-Path $source '*') -Destination $destination -Recurse -Force

        Get-ChildItem $destination -Recurse -File |
            Where-Object { $BuildOnlyExtensions -contains $_.Extension.ToLowerInvariant() } |
            Remove-Item -Force

        # Without the theme dictionary the surface's controls have no templates and draw nothing,
        # so a missing one fails the build here rather than on a customer's PC.
        foreach ($required in @('midiglass.exe', 'resources.pri', 'Themes\Generic.xbf', 'Microsoft.Graphics.Canvas.dll', 'Microsoft.WindowsAppRuntime.Bootstrap.dll', 'Windows.Devices.Midi2.dll')) {
            if (-not (Test-Path (Join-Path $destination $required))) {
                throw "Staged MIDI Glass is missing $required ($destination)"
            }
        }

        $files = @(Get-ChildItem $destination -Recurse -File)
        $xbf = @($files | Where-Object { $_.Extension -eq '.xbf' })

        Write-Detail "Staged $($files.Count) files ($($xbf.Count) xbf) -> midi-glass\$plat"

        # Before Setup, not after: WiX embeds these files into the MSI, so signing them afterwards
        # would sign a copy nobody installs.
        Invoke-SignPath -Path @($files | Where-Object { $_.Extension -in @('.exe', '.dll') } | ForEach-Object { $_.FullName })
    }
}

# ----------------------------------------------------------------------------------------------
# Installer file list
# ----------------------------------------------------------------------------------------------

# Ids that stay the same from one build to the next for the same file in the same folder, so the
# generated list does not churn in source control.
function Get-StableId {
    param([string] $Prefix, [string] $Value)

    $sha = [System.Security.Cryptography.SHA256]::Create()

    try {
        $bytes = $sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($Value.ToLowerInvariant()))
    }
    finally {
        $sha.Dispose()
    }

    return $Prefix + '_' + (($bytes[0..11] | ForEach-Object { $_.ToString('x2') }) -join '')
}

function New-AppFileList {
    # The same file names ship for every platform, so whichever platform was staged is enough.
    $listPlatform = $Platform | Where-Object { Test-Path (Join-Path $AppStagingRoot $_) } | Select-Object -First 1

    if (-not $listPlatform) {
        throw "Nothing staged under $AppStagingRoot. Run the Stage target first."
    }

    $stagedRoot = Join-Path $AppStagingRoot $listPlatform

    $directories = [System.Text.StringBuilder]::new()
    $components = [System.Text.StringBuilder]::new()
    $script:FileCount = 0

    function Add-Folder {
        param([string] $Folder, [string] $Relative, [string] $DirectoryId, [int] $Indent)

        foreach ($file in (Get-ChildItem $Folder -File | Sort-Object Name)) {
            $script:FileCount++

            $relativeFile = if ($Relative) { "$Relative\$($file.Name)" } else { $file.Name }
            $source = '$(StagingSourceRootFolder)\midi-glass\$(var.Platform)\' + $relativeFile

            $key = "$DirectoryId|$($file.Name)"
            $componentId = Get-StableId -Prefix 'c' -Value $key
            $fileId = if ($file.Name -ieq 'midiglass.exe' -and -not $Relative) { 'midiglassExe' } else { Get-StableId -Prefix 'f' -Value $key }

            # One file per component, which is both the Windows Installer guidance and the only
            # shape WiX generates a component GUID for. The GUID comes from the install path, so it
            # is the same one the App SDK runtime installer used while it carried MIDI Glass.
            [void]$components.AppendLine("      <Component Id=`"$componentId`" Bitness=`"always64`" Directory=`"$DirectoryId`"> <!-- $relativeFile -->")
            [void]$components.AppendLine("        <File Id=`"$fileId`" Source=`"$source`" Vital=`"true`" />")
            [void]$components.AppendLine('      </Component>')
        }

        foreach ($sub in (Get-ChildItem $Folder -Directory | Sort-Object Name)) {
            if (@(Get-ChildItem $sub.FullName -File -Recurse).Count -eq 0) { continue }

            $childId = ($DirectoryId + '_' + ($sub.Name -replace '[^A-Za-z0-9]', '_'))
            $childRelative = if ($Relative) { "$Relative\$($sub.Name)" } else { $sub.Name }
            $pad = ' ' * $Indent

            [void]$directories.AppendLine("$pad<Directory Id=`"$childId`" Name=`"$($sub.Name)`">")
            Add-Folder -Folder $sub.FullName -Relative $childRelative -DirectoryId $childId -Indent ($Indent + 2)
            [void]$directories.AppendLine("$pad</Directory>")
        }
    }

    Add-Folder -Folder $stagedRoot -Relative '' -DirectoryId $AppDirectoryId -Indent 6

    $sb = [System.Text.StringBuilder]::new()
    [void]$sb.AppendLine('<?xml version="1.0" encoding="utf-8"?>')
    [void]$sb.AppendLine('<!-- Generated by build\build-midi-glass.ps1 from the staged app. Do not edit; your changes will be overwritten. -->')
    [void]$sb.AppendLine('<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">')
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('  <?define StagingSourceRootFolder=$(env.MIDI_REPO_ROOT)\build\staging ?>')
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('  <Fragment>')

    if ($directories.Length -gt 0) {
        [void]$sb.AppendLine("    <DirectoryRef Id=`"$AppDirectoryId`">")
        [void]$sb.Append($directories.ToString())
        [void]$sb.AppendLine('    </DirectoryRef>')
        [void]$sb.AppendLine('')
    }

    [void]$sb.AppendLine('    <ComponentGroup Id="MidiGlassFiles">')
    [void]$sb.Append($components.ToString())
    [void]$sb.AppendLine('    </ComponentGroup>')
    [void]$sb.AppendLine('  </Fragment>')
    [void]$sb.AppendLine('</Wix>')

    Set-Content -Path $FileListFile -Value $sb.ToString() -Encoding UTF8
    Write-Detail "Generated $(Split-Path -Leaf $FileListFile) ($($script:FileCount) files, from $listPlatform)"
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

    if (-not (Test-Path $VersionIncludeFile)) {
        throw "Version include not found: $VersionIncludeFile. Run the Version target first."
    }

    New-AppFileList

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

    $folder = Join-Path $ReleaseRoot "midi-glass-$($Version.ReleaseLabel)"
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

    # A crash report gives a module name, a build timestamp and an offset. Without the pdb from
    # that exact build the offset cannot be turned back into a function, and the build output is
    # overwritten by the next build.
    foreach ($plat in $Platform) {
        $symbolFolder = Join-Path $folder "symbols\$plat"
        New-Item -ItemType Directory -Force -Path $symbolFolder | Out-Null

        $pdb = Join-Path $AppOutRoot "$plat\$Configuration\midiglass.pdb"

        if (Test-Path $pdb) {
            Copy-Item $pdb -Destination $symbolFolder -Force
            Write-Detail "Symbols -> symbols\$plat"
        }
        else {
            Write-Note "Symbols not found: $pdb"
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

    if (Test-Path $AppStagingRoot) {
        Remove-Item $AppStagingRoot -Recurse -Force
        Write-Detail "Removed $AppStagingRoot"
    }
}

# ----------------------------------------------------------------------------------------------
# Main
# ----------------------------------------------------------------------------------------------

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

# @() matters: a one-element array unrolls to a scalar through an if-expression, and StrictMode
# then fails the .Count test below.
$targets = @(if ($Target -contains 'All') { @('Version', 'App', 'Stage', 'Setup', 'Release') } else { $Target })

$parallelism = if ($MaxCpuCount -gt 0) { "$MaxCpuCount of $([Environment]::ProcessorCount)" } else { "all $([Environment]::ProcessorCount)" }

Write-Host ''
Write-Host 'Windows MIDI Services - MIDI Glass build' -ForegroundColor White
Write-Detail "Repo          $RepoRoot"
Write-Detail "Targets       $($targets -join ', ')"
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

if ($targets -contains 'Clean') {
    Invoke-CleanTarget
    if ($targets.Count -eq 1) { return }
}

# MSBuild and the compilers inherit this process's priority class, so setting it here is what
# keeps a build off the desktop's back. Restored below so an interactive shell is not left low.
$previousPriority = [System.Diagnostics.Process]::GetCurrentProcess().PriorityClass
[System.Diagnostics.Process]::GetCurrentProcess().PriorityClass = [System.Diagnostics.ProcessPriorityClass]$Priority

try {
    $version = Get-BuildVersion

    if ($targets -notcontains 'Version') {
        Write-Detail "Version       $($version.SemVer)"
    }

    if ($targets -contains 'App' -or $targets -contains 'Setup') {
        $script:MSBuild = Resolve-MSBuild
        Write-Detail "MSBuild       $script:MSBuild"
    }

    if ($targets -contains 'Version') { Invoke-VersionTarget $version }
    if ($targets -contains 'App') { Invoke-AppTarget }
    if ($targets -contains 'Stage') { Invoke-StageTarget }
    if ($targets -contains 'Setup') { Invoke-SetupTarget }
    if ($targets -contains 'Release') { Invoke-ReleaseTarget $version }
}
finally {
    [System.Diagnostics.Process]::GetCurrentProcess().PriorityClass = $previousPriority
}

$stopwatch.Stop()
Write-Host ''
Write-Host ("Done in {0:hh\:mm\:ss}" -f $stopwatch.Elapsed) -ForegroundColor Green
Write-Host ''

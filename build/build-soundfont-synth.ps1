<#
.SYNOPSIS
    Builds, tests and packages MIDI 2.0 SoundFont Synth as an MSIX for x64 and Arm64.

.DESCRIPTION
    MIDI 2.0 SoundFont Synth is a standalone app, not part of Windows and not part of the Tools
    installer, so it has its own solution and its own build:

      src/standalone-apps/soundfont-synth/MidiSoundFontSynth.sln
          engine/   the SoundFont 2 engine, MIDI decoding, MIDI-CI and audio output
          app/      the WinUI 3 app, midisoundfontsynth.exe
          tests/    TAEF tests for the engine

    The app gets the Windows MIDI Services SDK from the published NuGet package, the same way any
    other app does, and runs either from the MSIX or straight from its build folder.

    Steps, per platform: restore and build the solution, run the tests (x64 only, since this
    machine runs x64 test binaries), lay out the package, write its manifest from
    packaging/AppxManifest.template.xml and pack it with makeappx. With both platforms, the two
    packages are bundled into one .msixbundle, which is what the Store takes.

    Output goes to src/standalone-apps/soundfont-synth/VSFiles/package/.

.PARAMETER Platform
    x64, Arm64 or both. Defaults to both.

.PARAMETER Configuration
    Release (the default) or Debug.

.PARAMETER Version
    The package version, four parts. The last part must be 0 for the Store.

.PARAMETER IdentityName
    The package identity name. For the Store, use the one Partner Center reserved for the app.

.PARAMETER Publisher
    The package publisher. It has to match the subject of the certificate that signs the package
    exactly. For the Store, use the publisher Partner Center shows for the account; the Store signs
    the package itself.

.PARAMETER PublisherDisplayName
    The publisher name customers see.

.PARAMETER Sign
    Sign the packages with build/sign-files.ps1 and an Azure Artifact Signing certificate profile.
    Not needed for a Store submission. -Publisher must match the certificate's subject.

.PARAMETER SigningMetadata
    The Artifact Signing metadata JSON used by -Sign. Defaults to MIDI_SIGNING_METADATA.

.PARAMETER SkipTests
    Build and package without running the engine tests.

.PARAMETER Register
    Register the x64 package layout for the current user, so the packaged app can be tried
    without signing. Needs Developer Mode. Remove it again with
    Get-AppxPackage <IdentityName> | Remove-AppxPackage.

.EXAMPLE
    .\build-soundfont-synth.ps1
    Builds, tests and packs x64 and Arm64, and bundles them.

.EXAMPLE
    .\build-soundfont-synth.ps1 -Platform x64 -Register
    Builds the x64 package and registers its layout to try the packaged app on this PC.
#>
[CmdletBinding()]
param(
    [ValidateSet('x64', 'Arm64')]
    [string[]] $Platform = @('x64', 'Arm64'),

    [ValidateSet('Release', 'Debug')]
    [string] $Configuration = 'Release',

    [ValidatePattern('^\d+\.\d+\.\d+\.\d+$')]
    [string] $Version = '0.1.0.0',

    [string] $IdentityName = 'MIDI2SoundFontSynth',

    [string] $Publisher = 'CN=MIDI 2.0 SoundFont Synth Developer',

    [string] $PublisherDisplayName = 'Microsoft',

    [switch] $Sign,

    [string] $SigningMetadata = $env:MIDI_SIGNING_METADATA,

    [switch] $SkipTests,

    [switch] $Register
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$AppRoot = Join-Path $RepoRoot 'src\standalone-apps\soundfont-synth'
$Solution = Join-Path $AppRoot 'MidiSoundFontSynth.sln'
$ManifestTemplate = Join-Path $AppRoot 'packaging\AppxManifest.template.xml'
$IconSource = Join-Path $AppRoot 'packaging\AppIcon-source.png'
$PackageRoot = Join-Path $AppRoot 'VSFiles\package'

# Build output that never ships: symbols, import libraries, metadata and the incremental linker's files.
$BuildOnlyExtensions = @('.pdb', '.exp', '.lib', '.winmd', '.ipdb', '.iobj', '.ilk')

function Find-MSBuild
{
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

    if (Test-Path $vswhere)
    {
        $found = & $vswhere -latest -prerelease -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1

        if ($found) { return $found }
    }

    throw 'MSBuild was not found. Install Visual Studio with the C++ desktop and Windows App SDK workloads.'
}

function Find-SdkTool([string] $Name)
{
    $bin = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'

    $found = Get-ChildItem -Path $bin -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^10\.' } |
        Sort-Object { [version]$_.Name } -Descending |
        ForEach-Object { Join-Path $_.FullName "x64\$Name" } |
        Where-Object { Test-Path $_ } |
        Select-Object -First 1

    if (-not $found) { throw "$Name was not found in the Windows SDK." }

    return $found
}

function Invoke-Checked([string] $What, [scriptblock] $Command)
{
    Write-Host "== $What" -ForegroundColor Cyan

    & $Command

    if ($LASTEXITCODE -ne 0) { throw "$What failed with exit code $LASTEXITCODE." }
}

# The package logos are drawn from one square source, so a new icon means replacing one file.
function New-Logo([string] $Destination, [int] $Width, [int] $Height, [double] $IconFraction)
{
    Add-Type -AssemblyName System.Drawing

    $source = [System.Drawing.Image]::FromFile($IconSource)

    try
    {
        $bitmap = [System.Drawing.Bitmap]::new($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)

        try
        {
            $graphics.Clear([System.Drawing.Color]::Transparent)
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

            $size = [int][Math]::Round([Math]::Min($Width, $Height) * $IconFraction)
            $x = [int](($Width - $size) / 2)
            $y = [int](($Height - $size) / 2)

            $graphics.DrawImage($source, [System.Drawing.Rectangle]::new($x, $y, $size, $size))
        }
        finally
        {
            $graphics.Dispose()
        }

        $bitmap.Save($Destination, [System.Drawing.Imaging.ImageFormat]::Png)
        $bitmap.Dispose()
    }
    finally
    {
        $source.Dispose()
    }
}

function New-PackageLayout([string] $PlatformName, [string] $AppOutput, [string] $Layout)
{
    if (Test-Path $Layout) { Remove-Item $Layout -Recurse -Force }

    $null = New-Item -ItemType Directory -Path $Layout
    $null = New-Item -ItemType Directory -Path (Join-Path $Layout 'Assets')

    Get-ChildItem -Path $AppOutput -File |
        Where-Object { $BuildOnlyExtensions -notcontains $_.Extension.ToLowerInvariant() } |
        ForEach-Object { Copy-Item $_.FullName -Destination $Layout }

    # Unpackaged, MRT Core reads <exe>.pri. Packaged, it reads resources.pri.
    Copy-Item (Join-Path $AppOutput 'midisoundfontsynth.pri') -Destination (Join-Path $Layout 'resources.pri')

    New-Logo (Join-Path $Layout 'Assets\Square44x44Logo.png') 44 44 1.0
    New-Logo (Join-Path $Layout 'Assets\Square150x150Logo.png') 150 150 0.66
    New-Logo (Join-Path $Layout 'Assets\Wide310x150Logo.png') 310 150 0.66
    New-Logo (Join-Path $Layout 'Assets\StoreLogo.png') 50 50 1.0

    $architecture = if ($PlatformName -eq 'Arm64') { 'arm64' } else { 'x64' }

    $manifest = Get-Content -Path $ManifestTemplate -Raw
    $manifest = $manifest.Replace('$IdentityName$', [System.Security.SecurityElement]::Escape($IdentityName))
    $manifest = $manifest.Replace('$Publisher$', [System.Security.SecurityElement]::Escape($Publisher))
    $manifest = $manifest.Replace('$PublisherDisplayName$', [System.Security.SecurityElement]::Escape($PublisherDisplayName))
    $manifest = $manifest.Replace('$Version$', $Version)
    $manifest = $manifest.Replace('$Architecture$', $architecture)

    if ($manifest -match '\$[A-Za-z]+\$') { throw "The manifest still has an unfilled token: $($Matches[0])" }

    Set-Content -Path (Join-Path $Layout 'AppxManifest.xml') -Value $manifest -Encoding utf8NoBOM
}

$msbuild = Find-MSBuild
$makeappx = Find-SdkTool 'makeappx.exe'

Write-Host "MSBuild   $msbuild"
Write-Host "makeappx  $makeappx"
Write-Host "Version   $Version"
Write-Host "Identity  $IdentityName / $Publisher"

if ($Sign -and -not $SigningMetadata) { throw '-Sign needs -SigningMetadata or MIDI_SIGNING_METADATA.' }

$null = New-Item -ItemType Directory -Force -Path $PackageRoot

$packages = @()

Invoke-Checked 'Restore' { & $msbuild $Solution /t:Restore /p:Configuration=$Configuration /p:Platform=x64 /v:minimal /nologo }

foreach ($platformName in $Platform)
{
    Invoke-Checked "Build $Configuration $platformName" {
        & $msbuild $Solution /t:Build /p:Configuration=$Configuration /p:Platform=$platformName /p:PreferredToolArchitecture=x64 /m /v:minimal /nologo
    }

    if ($platformName -eq 'x64' -and -not $SkipTests)
    {
        $te = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Testing\Runtimes\TAEF\x64\TE.exe'
        $tests = Join-Path $AppRoot "VSFiles\x64\$Configuration\SoundFontSynthTests.dll"

        if (Test-Path $te)
        {
            Invoke-Checked 'Engine tests' { & $te $tests /logOutput:Low }
        }
        else
        {
            Write-Warning "TAEF was not found at $te, so the engine tests did not run."
        }
    }

    $appOutput = Join-Path $AppRoot "VSFiles\$platformName\$Configuration\midisoundfontsynth"
    $layout = Join-Path $PackageRoot "$platformName\layout"
    $msix = Join-Path $PackageRoot "MidiSoundFontSynth_$($Version)_$platformName.msix"

    Write-Host "== Lay out $platformName" -ForegroundColor Cyan
    New-PackageLayout $platformName $appOutput $layout

    Invoke-Checked "Pack $platformName" { & $makeappx pack /d $layout /p $msix /o }

    $packages += $msix
}

$outputs = @($packages)

if ($packages.Count -gt 1)
{
    $bundleInput = Join-Path $PackageRoot 'bundle-input'

    if (Test-Path $bundleInput) { Remove-Item $bundleInput -Recurse -Force }

    $null = New-Item -ItemType Directory -Path $bundleInput

    $packages | ForEach-Object { Copy-Item $_ -Destination $bundleInput }

    $bundle = Join-Path $PackageRoot "MidiSoundFontSynth_$Version.msixbundle"

    Invoke-Checked 'Bundle' { & $makeappx bundle /d $bundleInput /p $bundle /bv $Version /o }

    Remove-Item $bundleInput -Recurse -Force

    $outputs += $bundle
}

if ($Sign)
{
    Invoke-Checked 'Sign' { & (Join-Path $PSScriptRoot 'sign-files.ps1') -Path $outputs -MetadataFile $SigningMetadata -Description 'MIDI 2.0 SoundFont Synth' }
}

if ($Register)
{
    $layout = Join-Path $PackageRoot 'x64\layout'

    if (-not (Test-Path (Join-Path $layout 'AppxManifest.xml'))) { throw '-Register needs the x64 platform in the same run.' }

    Write-Host '== Register the x64 layout' -ForegroundColor Cyan
    Add-AppxPackage -Register (Join-Path $layout 'AppxManifest.xml') -ForceApplicationShutdown
}

Write-Host ''
Write-Host 'Packages:' -ForegroundColor Green
$outputs | ForEach-Object { Write-Host "  $_" }

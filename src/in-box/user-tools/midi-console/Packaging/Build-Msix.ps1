#Requires -Version 5.1
<#
.SYNOPSIS
    Builds a signed, self-contained, full-trust MSIX bundle for the Windows MIDI Console.

.DESCRIPTION
    Pipeline:
      1. dotnet publish Midi.csproj self-contained for win-x64 and win-arm64
      2. stage each publish output with a stamped Package.appxmanifest and the Images folder
      3. MakeAppx pack   -> one .msix per architecture
      4. MakeAppx bundle -> a single .msixbundle
      5. SignTool sign   -> signs the bundle

    The resulting package has no .NET runtime dependency, declares runFullTrust, and
    registers the "midi" app execution alias.

.PARAMETER Version
    Four-part package version. MSIX requires the revision (fourth) field to be 0.

.PARAMETER Configuration
    Build configuration passed to dotnet publish. Defaults to Release.

.PARAMETER OutputPath
    Directory that receives the .msix and .msixbundle outputs.

.PARAMETER Publisher
    Publisher distinguished name written into the manifest. Must exactly match the subject
    of the signing certificate, otherwise installation fails with 0x800B0109 / signature errors.

.PARAMETER CertificatePath
    Path to a .pfx used to sign. If omitted, a self-signed development certificate matching
    -Publisher is created (or reused) in CurrentUser\My and used instead.

.PARAMETER CertificatePassword
    Password for the .pfx supplied via -CertificatePath.

.PARAMETER TimestampUrl
    RFC3161 timestamp server used when signing.

.PARAMETER SkipSigning
    Produce an unsigned bundle. Use this in CI when signing happens in a separate, secured step.

.EXAMPLE
    .\Build-Msix.ps1
    Local developer build signed with a self-signed test certificate.

.EXAMPLE
    .\Build-Msix.ps1 -Version 1.2.0.0 -CertificatePath C:\certs\midi.pfx -CertificatePassword $pw
    Release build signed with the real certificate.
#>
[CmdletBinding()]
param(
    [ValidatePattern('^\d+\.\d+\.\d+\.0$')]
    [string] $Version = '1.0.0.0',

    [string] $Configuration = 'Release',

    [string] $OutputPath = (Join-Path $PSScriptRoot 'output'),

    [string] $Publisher = 'CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US',

    [string] $CertificatePath,

    [string] $CertificatePassword,

    [string] $TimestampUrl = 'http://timestamp.digicert.com',

    [switch] $SkipSigning
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$architectures  = @(
    [pscustomobject]@{ Msix = 'x64';   Rid = 'win-x64';   Platform = 'x64'   }
    [pscustomobject]@{ Msix = 'arm64'; Rid = 'win-arm64'; Platform = 'ARM64' }
)

$projectPath    = Join-Path $PSScriptRoot '..\Midi\Midi.csproj' | Resolve-Path
$manifestSource = Join-Path $PSScriptRoot 'Package.appxmanifest'
$imagesSource   = Join-Path $PSScriptRoot 'Images'
$stagingRoot    = Join-Path $PSScriptRoot 'obj\staging'
$bundleStaging  = Join-Path $PSScriptRoot 'obj\bundle'
$packageName    = 'WindowsMidiConsole'

function Get-WindowsSdkTool {
    param([Parameter(Mandatory)][string] $ToolName)

    $roots = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "$env:ProgramFiles\Windows Kits\10\bin"
    ) | Where-Object { $_ -and (Test-Path $_) }

    if (-not $roots) {
        throw "The Windows 10/11 SDK was not found. Install it (including 'MSIX Packaging Tools') to get $ToolName."
    }

    # Prefer the newest SDK version, and a host-architecture-matching binary.
    $hostArch = if ([Environment]::Is64BitOperatingSystem) {
        if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64') { 'arm64' } else { 'x64' }
    } else { 'x86' }

    $candidates =
        Get-ChildItem -Path $roots -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^10\.\d+\.\d+\.\d+$' } |
        Sort-Object { [version] $_.Name } -Descending |
        ForEach-Object {
            Join-Path $_.FullName "$hostArch\$ToolName"
            Join-Path $_.FullName "x64\$ToolName"
        }

    $tool = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1

    if (-not $tool) {
        throw "Could not locate $ToolName under the installed Windows SDK. Install the Windows SDK MSIX packaging tools."
    }

    return $tool
}

function Get-SigningCertificate {
    # Reuse an existing self-signed dev cert with the same subject if present, else create one.
    $existing =
        Get-ChildItem Cert:\CurrentUser\My |
        Where-Object { $_.Subject -eq $Publisher -and $_.HasPrivateKey -and $_.NotAfter -gt (Get-Date) } |
        Sort-Object NotAfter -Descending |
        Select-Object -First 1

    if ($existing) {
        Write-Host "  Reusing development certificate $($existing.Thumbprint)"
        return $existing
    }

    Write-Host "  Creating self-signed development certificate for $Publisher"
    $cert = New-SelfSignedCertificate `
        -Type Custom `
        -Subject $Publisher `
        -KeyUsage DigitalSignature `
        -FriendlyName 'Windows MIDI Console MSIX development certificate' `
        -CertStoreLocation 'Cert:\CurrentUser\My' `
        -NotAfter (Get-Date).AddYears(2) `
        -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3', '2.5.29.19={text}')

    Write-Warning 'A self-signed certificate was created. To install the package locally you must trust it:'
    Write-Warning "  Export it and import into Local Machine\Trusted People (run as administrator)."
    Write-Warning "  Thumbprint: $($cert.Thumbprint)"

    return $cert
}

Write-Host 'Windows MIDI Console - MSIX packaging' -ForegroundColor Cyan
Write-Host "  Version:       $Version"
Write-Host "  Configuration: $Configuration"
Write-Host "  Publisher:     $Publisher"
Write-Host ''

$makeAppx = Get-WindowsSdkTool -ToolName 'makeappx.exe'
$signTool = Get-WindowsSdkTool -ToolName 'signtool.exe'

foreach ($dir in @($stagingRoot, $bundleStaging, $OutputPath)) {
    if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
}

$manifestTemplate = Get-Content $manifestSource -Raw

foreach ($arch in $architectures) {

    Write-Host "Publishing $($arch.Rid) (self-contained)..." -ForegroundColor Green

    $publishDir = Join-Path $stagingRoot $arch.Msix

    & dotnet publish $projectPath `
        --configuration $Configuration `
        --runtime $arch.Rid `
        --self-contained true `
        -p:Platform=$($arch.Platform) `
        -p:PublishSingleFile=false `
        -p:PublishReadyToRun=false `
        -p:Version=$Version `
        --output $publishDir

    if ($LASTEXITCODE -ne 0) {
        throw "dotnet publish failed for $($arch.Rid) with exit code $LASTEXITCODE."
    }

    # Publishing leaves behind artifacts that must not ship inside the package.
    Get-ChildItem $publishDir -Include '*.pdb', '*.xml', '*.config.dev.json' -Recurse -File |
        Remove-Item -Force -ErrorAction SilentlyContinue

    if (-not (Test-Path (Join-Path $publishDir 'midi.exe'))) {
        throw "midi.exe was not found in the publish output for $($arch.Rid)."
    }

    Write-Host "  Staging manifest and images for $($arch.Msix)..."

    Copy-Item $imagesSource -Destination (Join-Path $publishDir 'Images') -Recurse -Force

    $manifest = $manifestTemplate.
        Replace('$Version$', $Version).
        Replace('$Architecture$', $arch.Msix).
        Replace('$Publisher$', $Publisher)

    # AppxManifest.xml (not .appxmanifest) is the name MakeAppx expects in a layout folder.
    Set-Content -Path (Join-Path $publishDir 'AppxManifest.xml') -Value $manifest -Encoding UTF8

    $msixPath = Join-Path $bundleStaging "$packageName-$Version-$($arch.Msix).msix"

    Write-Host "  Packing $(Split-Path $msixPath -Leaf)..."
    $packOutput = & $makeAppx pack /o /d $publishDir /p $msixPath 2>&1

    if ($LASTEXITCODE -ne 0) {
        $packOutput | Write-Host
        throw "makeappx pack failed for $($arch.Msix) with exit code $LASTEXITCODE."
    }

    Copy-Item $msixPath -Destination $OutputPath -Force
}

$bundlePath = Join-Path $OutputPath "$packageName-$Version.msixbundle"

Write-Host "Bundling x64 and arm64 into $(Split-Path $bundlePath -Leaf)..." -ForegroundColor Green
$bundleOutput = & $makeAppx bundle /o /bv $Version /d $bundleStaging /p $bundlePath 2>&1

if ($LASTEXITCODE -ne 0) {
    $bundleOutput | Write-Host
    throw "makeappx bundle failed with exit code $LASTEXITCODE."
}

if ($SkipSigning) {
    Write-Host ''
    Write-Host 'Skipping signing as requested. The bundle is unsigned and cannot be installed as-is.' -ForegroundColor Yellow
}
else {
    Write-Host 'Signing the bundle...' -ForegroundColor Green

    if ($CertificatePath) {
        if (-not (Test-Path $CertificatePath)) {
            throw "Certificate file not found: $CertificatePath"
        }

        $signArgs = @('sign', '/fd', 'SHA256', '/tr', $TimestampUrl, '/td', 'SHA256', '/f', $CertificatePath)
        if ($CertificatePassword) { $signArgs += @('/p', $CertificatePassword) }
        $signArgs += $bundlePath
    }
    else {
        $cert = Get-SigningCertificate
        $signArgs = @('sign', '/fd', 'SHA256', '/sha1', $cert.Thumbprint, '/s', 'My', $bundlePath)
    }

    $signOutput = & $signTool @signArgs 2>&1

    if ($LASTEXITCODE -ne 0) {
        $signOutput | Write-Host
        throw "signtool failed with exit code $LASTEXITCODE. Verify that the certificate subject exactly matches -Publisher ('$Publisher')."
    }
}

Write-Host ''
Write-Host 'Done.' -ForegroundColor Cyan
Write-Host "  Bundle: $bundlePath"
Get-ChildItem $OutputPath | ForEach-Object { Write-Host "    $($_.Name)" }

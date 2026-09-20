<#
.SYNOPSIS
    Authenticode-signs build output with an Azure Artifact Signing certificate profile.

.DESCRIPTION
    Wraps signtool.exe and the Artifact Signing dlib (formerly Trusted Signing / Azure Code
    Signing) so the release scripts and the WiX projects all sign the same way.

    Signing is never implicit. Every caller passes -MetadataFile, or sets MIDI_SIGNING_METADATA,
    and nothing here runs unless one of those is present.

    Files that already carry a valid signature are skipped, so redistributables that Microsoft
    already signed - the VC runtime, the Windows App Runtime bootstrapper, WebView2, the .NET
    runtime assemblies pulled in by a publish - keep their original signature instead of being
    re-signed with ours. Pass -Force to override that.

    Artifact Signing certificates are valid for three days, so the timestamp is not optional: an
    un-timestamped signature stops validating almost immediately.

.PARAMETER Path
    Files and/or folders. Folders are walked recursively and filtered to signable file types.

.PARAMETER MetadataFile
    The Artifact Signing metadata JSON (Endpoint, CodeSigningAccountName, CertificateProfileName).
    Defaults to the MIDI_SIGNING_METADATA environment variable.

.PARAMETER SignToolPath
    signtool.exe from the Windows SDK, 10.0.2261.755 or newer. Defaults to MIDI_SIGNTOOL, then to
    the newest Windows Kits install.

.PARAMETER DlibPath
    Azure.CodeSigning.Dlib.dll from the Artifact Signing client tools. Defaults to
    MIDI_SIGNING_DLIB, then to the installed client tools, then to the extracted NuGet package.

.EXAMPLE
    .\sign-files.ps1 -Path .\staging\app-sdk -MetadataFile C:\signing\metadata.json

.EXAMPLE
    $env:MIDI_SIGNING_METADATA = 'C:\signing\metadata.json'
    .\build-sdk.ps1 -Sign

.NOTES
    Set up once per machine:

        winget install -e --id Microsoft.Azure.ArtifactSigningClientTools

    then write a metadata file containing the account endpoint, account name and profile name, and
    sign in with an identity that holds the Certificate Profile Signer role (az login, a managed
    identity, or the AZURE_TENANT_ID / AZURE_CLIENT_ID / AZURE_CLIENT_SECRET environment
    variables). The dlib authenticates with DefaultAzureCredential.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory, Position = 0)]
    [string[]] $Path,

    [string] $MetadataFile = $env:MIDI_SIGNING_METADATA,

    [string] $SignToolPath = $env:MIDI_SIGNTOOL,

    [string] $DlibPath = $env:MIDI_SIGNING_DLIB,

    [string] $TimestampUrl = 'http://timestamp.acs.microsoft.com',

    [string] $Description = 'Windows MIDI Services',

    [string] $DescriptionUrl = 'https://aka.ms/midi',

    # Re-sign files that already have a valid signature. Off by default so third-party
    # redistributables keep the signature they shipped with.
    [switch] $Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# PE files, installer formats, and the script types the PowerShell SIP understands. Extensions
# that cannot carry an Authenticode signature (.cmd, .json, .pri, .xbf, .pdb) are ignored rather
# than failing the build, because callers hand whole staging folders to this script.
$SignableExtensions = @(
    '.exe', '.dll', '.sys', '.ocx', '.efi',
    '.msi', '.msm', '.msp', '.msix', '.appx', '.cab', '.cat',
    '.ps1', '.psm1', '.psd1'
)

# signtool takes many files per invocation and each invocation costs a service round trip, so
# batch. Kept well under the command-line limit even with long staging paths.
$BatchSize = 40

function Get-HostArchitecture {
    if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64') { return 'arm64' }
    return 'x64'
}

function Resolve-SignTool {
    if ($SignToolPath) {
        if (-not (Test-Path $SignToolPath)) { throw "signtool.exe not found at -SignToolPath: $SignToolPath" }
        return (Resolve-Path $SignToolPath).Path
    }

    $arch = Get-HostArchitecture

    $candidates = @(
        (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'),
        (Join-Path $env:ProgramFiles 'Windows Kits\10\bin')
    ) | Where-Object { $_ -and (Test-Path $_) }

    $found = foreach ($root in $candidates) {
        Get-ChildItem $root -Directory -Filter '10.*' -ErrorAction SilentlyContinue |
            ForEach-Object {
                $exe = Join-Path $_.FullName "$arch\signtool.exe"
                if (Test-Path $exe) {
                    [pscustomobject]@{ Version = [version]$_.Name; Path = $exe }
                }
            }
    }

    $best = @($found) | Sort-Object Version -Descending | Select-Object -First 1
    if (-not $best) {
        throw "Could not find signtool.exe for $arch under Windows Kits\10\bin. Install the Windows SDK, or pass -SignToolPath / set MIDI_SIGNTOOL."
    }

    return $best.Path
}

function Resolve-Dlib {
    if ($DlibPath) {
        if (-not (Test-Path $DlibPath)) { throw "Azure.CodeSigning.Dlib.dll not found at -DlibPath: $DlibPath" }
        return (Resolve-Path $DlibPath).Path
    }

    $arch = Get-HostArchitecture

    # The winget/MSI client tools drop the dlib flat in a per-user folder; the NuGet package
    # splits it by architecture. Both layouts, under the current and the former package names.
    $patterns = @(
        (Join-Path $env:LOCALAPPDATA 'Microsoft\*ArtifactSigning*\Azure.CodeSigning.Dlib.dll'),
        (Join-Path $env:LOCALAPPDATA 'Microsoft\*Signing Client Tools*\Azure.CodeSigning.Dlib.dll'),
        (Join-Path $env:ProgramFiles "*Signing Client Tools\bin\$arch\Azure.CodeSigning.Dlib.dll"),
        (Join-Path $env:ProgramFiles "Microsoft\*Signing Client Tools\bin\$arch\Azure.CodeSigning.Dlib.dll"),
        (Join-Path ${env:ProgramFiles(x86)} "*Signing Client Tools\bin\$arch\Azure.CodeSigning.Dlib.dll"),
        (Join-Path $env:USERPROFILE ".nuget\packages\microsoft.artifactsigning.client\*\bin\$arch\Azure.CodeSigning.Dlib.dll"),
        (Join-Path $env:USERPROFILE ".nuget\packages\microsoft.trusted.signing.client\*\bin\$arch\Azure.CodeSigning.Dlib.dll"),
        (Join-Path $PSScriptRoot "dependencies\signing\$arch\Azure.CodeSigning.Dlib.dll")
    )

    foreach ($pattern in $patterns) {
        $match = @(Get-ChildItem $pattern -ErrorAction SilentlyContinue | Sort-Object FullName -Descending) |
            Select-Object -First 1
        if ($match) { return $match.FullName }
    }

    throw @"
Could not find Azure.CodeSigning.Dlib.dll for $arch.

Install the Artifact Signing client tools:

    winget install -e --id Microsoft.Azure.ArtifactSigningClientTools

or pass -DlibPath / set MIDI_SIGNING_DLIB to the dlib in an extracted
Microsoft.ArtifactSigning.Client NuGet package.
"@
}

function Get-FileToSign {
    param([Parameter(Mandatory)] [string[]] $InputPath)

    $files = foreach ($item in $InputPath) {
        if (-not (Test-Path $item)) { throw "Path to sign does not exist: $item" }

        if (Test-Path $item -PathType Container) {
            Get-ChildItem $item -File -Recurse
        }
        else {
            Get-Item $item
        }
    }

    @($files) |
        Where-Object { $SignableExtensions -contains $_.Extension.ToLowerInvariant() } |
        Sort-Object FullName -Unique
}

# The dlib reports a bad metadata file as an unhandled UriFormatException behind
# "SignerSign() failed", which says nothing about the cause. Check it here instead.
function Assert-MetadataUsable {
    param([Parameter(Mandatory)] [string] $File)

    try {
        $metadata = Get-Content $File -Raw | ConvertFrom-Json
    }
    catch {
        throw "Artifact Signing metadata is not valid JSON: $File"
    }

    foreach ($name in @('Endpoint', 'CodeSigningAccountName', 'CertificateProfileName')) {
        $value = if ($metadata.PSObject.Properties.Name -contains $name) { $metadata.$name } else { $null }

        if (-not $value) {
            throw "Artifact Signing metadata is missing '$name': $File"
        }

        # metadata.sample.json ships every value as <Placeholder Text>.
        if ($value -match '^<.*>$') {
            throw "Artifact Signing metadata still holds the sample placeholder for '$name': $File`nFill in the endpoint, account name and certificate profile name from the Azure portal."
        }
    }

    if (-not [uri]::IsWellFormedUriString($metadata.Endpoint, [UriKind]::Absolute)) {
        throw "Artifact Signing 'Endpoint' is not an absolute URI: '$($metadata.Endpoint)' in $File`nUse the region URI for the account, for example https://wus2.codesigning.azure.net."
    }
}

# ----------------------------------------------------------------------------------------------

if (-not $MetadataFile) {
    throw 'No Artifact Signing metadata. Pass -MetadataFile or set MIDI_SIGNING_METADATA to the JSON file holding Endpoint, CodeSigningAccountName and CertificateProfileName.'
}
if (-not (Test-Path $MetadataFile)) {
    throw "Artifact Signing metadata file not found: $MetadataFile"
}
$MetadataFile = (Resolve-Path $MetadataFile).Path

Assert-MetadataUsable -File $MetadataFile

$signTool = Resolve-SignTool
$dlib = Resolve-Dlib

$candidates = @(Get-FileToSign -InputPath $Path)

$skipped = @()
if (-not $Force) {
    $unsigned = foreach ($file in $candidates) {
        if ((Get-AuthenticodeSignature -FilePath $file.FullName).Status -eq 'Valid') {
            $skipped += $file
        }
        else {
            $file
        }
    }
    $candidates = @($unsigned)
}

if ($candidates.Count -eq 0) {
    Write-Host "     Signing: nothing to do ($($skipped.Count) already signed)" -ForegroundColor DarkGray
    return
}

Write-Host "     Signing $($candidates.Count) files ($($skipped.Count) already signed, left alone)" -ForegroundColor DarkGray

$commonArgs = @(
    'sign'
    '/v'
    '/fd', 'SHA256'
    '/tr', $TimestampUrl
    '/td', 'SHA256'
    '/dlib', $dlib
    '/dmdf', $MetadataFile
    '/d', $Description
    '/du', $DescriptionUrl
)

for ($i = 0; $i -lt $candidates.Count; $i += $BatchSize) {
    $batch = @($candidates[$i..([Math]::Min($i + $BatchSize, $candidates.Count) - 1)])

    # Built as one array and splatted: handing a native command a nested array argument is where
    # paths with spaces stop being separate arguments.
    $arguments = $commonArgs + @($batch | ForEach-Object { $_.FullName })

    & $signTool @arguments

    if ($LASTEXITCODE -ne 0) {
        throw @"
signtool failed ($LASTEXITCODE) signing $($batch.Count) files starting with $($batch[0].FullName)

If the dlib reported 403 Forbidden, the two usual causes are:
  * "Endpoint" in $MetadataFile names a different region than the signing account. It must be the
    region URI for the account itself, for example https://eus.codesigning.azure.net for East US.
  * The signed-in identity does not hold the Certificate Profile Signer role on that profile.
"@
    }
}

Write-Host "     Signed $($candidates.Count) files" -ForegroundColor DarkGray

# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License
# ============================================================================
# Windows MIDI Services
# Network MIDI 2.0 (UDP) configuration repair tool
# https://github.com/microsoft/MIDI/
# ============================================================================

<#
.SYNOPSIS
    Repairs, or removes, the Network MIDI 2.0 (UDP) entries in the Windows MIDI Services
    configuration file.

.DESCRIPTION
    Hand-edited configuration files, and files written by older previews of the API,
    often contain Network MIDI 2.0 entries the current service will refuse. A refused
    entry is skipped silently, so a host or a client simply never appears.

    This script backs the file up, then either repairs the Network MIDI 2.0 section or
    replaces it with an empty, comment-only section. Nothing outside the Network MIDI 2.0
    transport section is changed.

.PARAMETER ConfigFilePath
    Full path to the configuration file. Defaults to the machine configuration file.

.PARAMETER Action
    Repair or RemoveAll. The default, Ask, prompts.

.PARAMETER ReportOnly
    Analyze and report, but never write anything. No backup is taken in this mode.

.PARAMETER Force
    Skip the confirmation prompts. Requires an explicit -Action.

.EXAMPLE
    .\repair-network-config.ps1

.EXAMPLE
    .\repair-network-config.ps1 -ReportOnly
#>

#Requires -Version 5.1

[CmdletBinding()]
param
(
    [string] $ConfigFilePath = (Join-Path $env:ProgramData 'Microsoft\MIDI\WindowsMidiServices.midiconfig.json'),

    [ValidateSet('Ask', 'Repair', 'RemoveAll')]
    [string] $Action = 'Ask',

    [switch] $ReportOnly,

    [switch] $Force
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0


# ============================================================================
# What the service currently accepts. Sources:
#   src\in-box\Transport\UdpNetworkMidi2Transport\network_json_defs.h
#   src\in-box\Transport\UdpNetworkMidi2Transport\net2udp_transport_defs.h
#   src\in-box\Transport\UdpNetworkMidi2Transport\Midi2.NetworkMidiConfigurationManager.cpp
#   src\in-box\Client\WinRT\core\MidiNetworkHostCreationConfig.cpp
# ============================================================================

$script:TransportPluginSettingsKey = 'endpointTransportPluginSettings'
$script:NetworkTransportId         = '{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}'

# IDS_PLUGIN_METADATA_DESCRIPTION from Midi2.NetworkMidiTransport.rc
$script:NetworkTransportComment    = 'Provides host and client Network MIDI 2.0 (UDP) capabilities'

# MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT / MIDI_MAX_UMP_PRODUCT_INSTANCE_ID_BYTE_COUNT
$script:MaxEndpointNameBytes       = 98
$script:MaxProductInstanceIdBytes  = 42

# MIDI_DNSSD_SERVICE_INSTANCE_NAME_MAX_BYTE_COUNT, the RFC 1035 single label limit
$script:MaxServiceInstanceNameBytes = 63

# Keys the transport reads at the section level. "remove" is a command shape which the SDK
# applies and never persists, so a copy sitting in the file is a leftover.
$script:SectionKeyOrder = @('_comment', 'transportSettings', 'create', 'update')

# ReadClampedTransportSetting call sites, with the bounds from net2udp_transport_defs.h
$script:TransportSettingBounds = [ordered]@{
    'maxForwardErrorCorrectionCommandPackets' = @{ Default =      2; Minimum =    0; Maximum =     10 }
    'maxRetransmitBufferCommandPackets'       = @{ Default =     50; Minimum =    0; Maximum =   1000 }
    'outboundPingInterval'                    = @{ Default =   2000; Minimum =  250; Maximum = 120000 }
    'directConnectionScanInterval'            = @{ Default =  20000; Minimum =  250; Maximum = 300000 }
    'maxHostConnections'                      = @{ Default =     64; Minimum =    1; Maximum =    512 }
    'invitationPendingTimeout'                = @{ Default = 120000; Minimum = 1000; Maximum = 600000 }
}

# Declared in network_json_defs.h but with no consumer in the transport yet. Kept, because it is
# a current key rather than an obsolete one, but it does nothing today.
$script:ReservedTransportSettingKeys = @('networkInterface')

$script:TransportSettingKeyOrder = @($script:TransportSettingBounds.Keys) + $script:ReservedTransportSettingKeys

$script:HostKeyOrder = @(
    '_comment',
    'name',
    'serviceInstanceName',
    'productInstanceId',
    'networkProtocol',
    'port',
    'allowPortFallback',
    'enabled',
    'advertise',
    'createMidi1Ports',
    'customEndpointName',
    'authentication',
    'globalPassword',
    'userAuth',
    'remoteClientPolicy',
    'allowedClients',
    'deniedClients'
)

$script:ClientKeyOrder = @(
    '_comment',
    'networkProtocol',
    'enabled',
    'createMidi1Ports',
    'customEndpointName',
    'match'
)

# Only these five are read by the endpoint creator. "serviceInstance" is declared but unread.
$script:MatchKeyOrder = @(
    'id',
    'umpEndpointName',
    'umpProductInstanceId',
    'directHostNameOrIP',
    'directPort',
    'serviceInstance'
)

$script:UsableMatchKeys = @('id', 'umpEndpointName', 'umpProductInstanceId', 'directHostNameOrIP')

$script:Changes  = New-Object System.Collections.Generic.List[object]
$script:Warnings = New-Object System.Collections.Generic.List[object]


# ============================================================================
# Console output
# ============================================================================

function Write-Rule
{
    Write-Host ''.PadRight(78, '=') -ForegroundColor DarkGray
}

function Write-Banner([string] $Text)
{
    Write-Host ''
    Write-Rule
    Write-Host $Text -ForegroundColor DarkCyan
    Write-Rule
}

function Add-Change([string] $Scope, [string] $Message)
{
    $script:Changes.Add([pscustomobject]@{ Scope = $Scope; Message = $Message })
}

function Add-Warning([string] $Scope, [string] $Message)
{
    $script:Warnings.Add([pscustomobject]@{ Scope = $Scope; Message = $Message })
}

function Read-YesNo([string] $Prompt, [bool] $DefaultYes = $false)
{
    $suffix = if ($DefaultYes) { '[Y/n]' } else { '[y/N]' }

    while ($true)
    {
        $answer = (Read-Host "$Prompt $suffix").Trim().ToLowerInvariant()

        if ($answer -eq '')    { return $DefaultYes }
        if ($answer -in @('y', 'yes')) { return $true }
        if ($answer -in @('n', 'no'))  { return $false }

        Write-Host 'Please answer y or n.' -ForegroundColor Yellow
    }
}


# ============================================================================
# JSON. The file is parsed into ordinal-comparer ordered dictionaries and generic lists, then
# written back with the same four space indent, UTF-8 without a BOM, and LF line endings the
# SDK's own writer uses (MidiConfigFile.cpp AppendPretty). ConvertTo-Json is not used: it
# reorders nothing but does escape non-ASCII differently between Windows PowerShell and
# PowerShell 7, and its default depth would silently truncate the file.
#
# An ordinal comparer matters here. PowerShell's [ordered]@{} is case-insensitive, which would
# quietly merge two GUID keys differing only in case instead of reporting them.
# ============================================================================

function New-JsonObjectNode
{
    return , ([System.Collections.Specialized.OrderedDictionary]::new([System.StringComparer]::Ordinal))
}

function New-JsonArrayNode
{
    return , (New-Object System.Collections.Generic.List[object])
}

function Test-JsonObjectNode($Node)
{
    return ($null -ne $Node) -and ($Node -is [System.Collections.IDictionary])
}

function Test-JsonArrayNode($Node)
{
    return ($null -ne $Node) -and ($Node -is [System.Collections.IList]) -and ($Node -isnot [string])
}

function ConvertTo-JsonNode
{
    param([AllowNull()] $Value)

    if ($null -eq $Value)
    {
        return $null
    }

    if ($Value -is [string] -or $Value -is [bool] -or $Value.GetType().IsPrimitive -or $Value -is [decimal])
    {
        return $Value
    }

    if ($Value -is [System.Management.Automation.PSCustomObject] -or $Value -is [psobject])
    {
        $node = New-JsonObjectNode

        foreach ($property in $Value.PSObject.Properties)
        {
            if ($node.Contains($property.Name))
            {
                Add-Warning 'file' "Duplicate JSON key '$($property.Name)'. Only the first was kept."
                continue
            }

            $node[$property.Name] = ConvertTo-JsonNode -Value $property.Value
        }

        return , $node
    }

    if ($Value -is [System.Collections.IEnumerable])
    {
        $list = New-JsonArrayNode

        foreach ($item in $Value)
        {
            $list.Add((ConvertTo-JsonNode -Value $item))
        }

        return , $list
    }

    return $Value
}

function ConvertTo-JsonStringLiteral([string] $Value)
{
    $builder = New-Object System.Text.StringBuilder
    [void]$builder.Append('"')

    foreach ($character in $Value.ToCharArray())
    {
        $code = [int]$character

        if     ($character -eq '"')  { [void]$builder.Append('\"') }
        elseif ($character -eq '\')  { [void]$builder.Append('\\') }
        elseif ($code -eq 8)         { [void]$builder.Append('\b') }
        elseif ($code -eq 9)         { [void]$builder.Append('\t') }
        elseif ($code -eq 10)        { [void]$builder.Append('\n') }
        elseif ($code -eq 12)        { [void]$builder.Append('\f') }
        elseif ($code -eq 13)        { [void]$builder.Append('\r') }
        elseif ($code -lt 32)        { [void]$builder.AppendFormat('\u{0:x4}', $code) }
        else                         { [void]$builder.Append($character) }
    }

    [void]$builder.Append('"')

    return $builder.ToString()
}

function ConvertTo-JsonText
{
    param($Node, [int] $Depth = 0)

    $pad      = ' ' * (4 * $Depth)
    $padInner = ' ' * (4 * ($Depth + 1))

    if ($null -eq $Node)
    {
        return 'null'
    }

    if ($Node -is [bool])
    {
        return $(if ($Node) { 'true' } else { 'false' })
    }

    if ($Node -is [string])
    {
        return ConvertTo-JsonStringLiteral $Node
    }

    if (Test-JsonObjectNode $Node)
    {
        if ($Node.Count -eq 0)
        {
            return '{}'
        }

        $lines = foreach ($key in @($Node.Keys))
        {
            '{0}{1}: {2}' -f $padInner, (ConvertTo-JsonStringLiteral ([string]$key)), (ConvertTo-JsonText -Node $Node[$key] -Depth ($Depth + 1))
        }

        return "{`n" + ($lines -join ",`n") + "`n$pad}"
    }

    if (Test-JsonArrayNode $Node)
    {
        if ($Node.Count -eq 0)
        {
            return '[]'
        }

        $lines = foreach ($item in $Node)
        {
            $padInner + (ConvertTo-JsonText -Node $item -Depth ($Depth + 1))
        }

        return "[`n" + ($lines -join ",`n") + "`n$pad]"
    }

    if ($Node -is [double] -or $Node -is [single] -or $Node -is [decimal])
    {
        return ([double]$Node).ToString('R', [cultureinfo]::InvariantCulture)
    }

    return $Node.ToString([cultureinfo]::InvariantCulture)
}


# ============================================================================
# Value helpers
# ============================================================================

function Get-Utf8ByteCount([string] $Value)
{
    return [System.Text.Encoding]::UTF8.GetByteCount($Value)
}

# Truncates on text element boundaries so a surrogate pair or a combining sequence is never
# cut in half.
function Limit-Utf8ByteCount([string] $Value, [int] $MaxBytes)
{
    if ((Get-Utf8ByteCount $Value) -le $MaxBytes)
    {
        return $Value
    }

    $builder    = New-Object System.Text.StringBuilder
    $byteCount  = 0
    $enumerator = [System.Globalization.StringInfo]::GetTextElementEnumerator($Value)

    while ($enumerator.MoveNext())
    {
        $element   = [string]$enumerator.Current
        $elementSize = Get-Utf8ByteCount $element

        if (($byteCount + $elementSize) -gt $MaxBytes)
        {
            break
        }

        [void]$builder.Append($element)
        $byteCount += $elementSize
    }

    return $builder.ToString().Trim()
}

function Test-PrintableAscii([string] $Value)
{
    foreach ($character in $Value.ToCharArray())
    {
        $code = [int]$character

        if ($code -lt 0x20 -or $code -gt 0x7E)
        {
            return $false
        }
    }

    return $true
}

# winrt::guid's string constructor takes the braced and unbraced forms, and the SDK writes the
# braced lowercase form (winrt::to_hstring on a guid). Anything else is normalized to that.
function Get-CanonicalGuidKey([string] $Value)
{
    $parsed = [guid]::Empty

    if ([guid]::TryParse($Value.Trim(), [ref]$parsed))
    {
        return $parsed.ToString('B').ToLowerInvariant()
    }

    return $null
}

function Get-StringValue($Node, [string] $Key)
{
    if (-not (Test-JsonObjectNode $Node) -or -not $Node.Contains($Key))
    {
        return $null
    }

    $value = $Node[$Key]

    if ($value -is [string])
    {
        return $value.Trim()
    }

    return $null
}

# Sorts the keys a node already has into the documented order, leaving any it does not have out.
function Set-NodeKeyOrder($Node, [string[]] $Order)
{
    if (-not (Test-JsonObjectNode $Node))
    {
        return
    }

    $ordered = New-JsonObjectNode

    foreach ($key in $Order)
    {
        if ($Node.Contains($key))
        {
            $ordered[$key] = $Node[$key]
        }
    }

    foreach ($key in @($Node.Keys))
    {
        if (-not $ordered.Contains($key))
        {
            $ordered[$key] = $Node[$key]
        }
    }

    foreach ($key in @($Node.Keys))
    {
        $Node.Remove($key)
    }

    foreach ($key in @($ordered.Keys))
    {
        $Node[$key] = $ordered[$key]
    }
}

# Removes every key the transport does not read. Values from previous API versions, such as the
# old connectionPolicyIpv4 address lists, land here.
#
# -cnotcontains, not -notcontains. Configuration keys are case-sensitive, so "Name" is not the
# "name" the transport reads and has to go the same way as any other unrecognized key.
function Remove-UnknownKeys($Node, [string[]] $Known, [string] $Scope)
{
    foreach ($key in @($Node.Keys))
    {
        if ($Known -cnotcontains $key)
        {
            $Node.Remove($key)
            Add-Change $Scope "Removed unused property '$key'. The transport does not read it."
        }
    }
}

# The transport reads these with SafeGetNamedBoolean, which falls back to the default on any
# other type. A string "true" or "false" is a common hand-edit and is worth converting rather
# than discarding.
function Repair-BooleanProperty($Node, [string] $Key, [string] $Scope)
{
    if (-not $Node.Contains($Key))
    {
        return
    }

    $value = $Node[$Key]

    if ($value -is [bool])
    {
        return
    }

    if ($value -is [string])
    {
        $text = $value.Trim().ToLowerInvariant()

        if ($text -eq 'true' -or $text -eq 'false')
        {
            $Node[$Key] = ($text -eq 'true')
            Add-Change $Scope "Converted '$Key' from the string ""$value"" to a JSON boolean."
            return
        }
    }

    $Node.Remove($Key)
    Add-Change $Scope "Removed '$Key'. It was not a boolean, so the service default applies."
}

function Repair-TextProperty($Node, [string] $Key, [int] $MaxBytes, [string] $Scope)
{
    if (-not $Node.Contains($Key))
    {
        return
    }

    $value = $Node[$Key]

    if ($value -isnot [string])
    {
        $Node.Remove($Key)
        Add-Change $Scope "Removed '$Key'. It was not a string."
        return
    }

    $trimmed = $value.Trim()

    if ($trimmed -eq '')
    {
        $Node.Remove($Key)
        Add-Change $Scope "Removed empty '$Key'."
        return
    }

    $limited = Limit-Utf8ByteCount $trimmed $MaxBytes

    if ($limited -cne $value)
    {
        $Node[$Key] = $limited

        if ($limited.Length -lt $trimmed.Length)
        {
            Add-Change $Scope "Truncated '$Key' to the $MaxBytes UTF-8 byte limit."
        }
        else
        {
            Add-Change $Scope "Trimmed surrounding whitespace from '$Key'."
        }
    }
}

# "auto", an empty string and "0" all mean automatic allocation. Anything else has to be a real
# port number, or ValidateHostDefinition refuses the whole entry.
function Get-NormalizedPortText($Value)
{
    if ($null -eq $Value)
    {
        return $null
    }

    if ($Value -is [bool])
    {
        return $null
    }

    $text = ([string]$Value).Trim()

    if ($text -eq '' -or $text -eq '0' -or $text.ToLowerInvariant() -eq 'auto')
    {
        return 'auto'
    }

    if ($text -notmatch '^\d+$')
    {
        return $null
    }

    $number = [uint64]$text

    if ($number -lt 1 -or $number -gt 65535)
    {
        return $null
    }

    return $number.ToString([cultureinfo]::InvariantCulture)
}

# EnsureCompliantServiceInstanceName in the SDK: a period would split the DNS-SD label in two,
# and C0, DEL and C1 are excluded by Net-Unicode. Spaces and non-ASCII are legal and are kept.
function Get-CompliantServiceInstanceName([string] $Value)
{
    $builder = New-Object System.Text.StringBuilder

    foreach ($character in $Value.Trim().ToCharArray())
    {
        $code = [int]$character

        if ($character -eq '.')                          { continue }
        if ($code -lt 0x20 -or $code -eq 0x7F)           { continue }
        if ($code -ge 0x80 -and $code -le 0x9F)          { continue }

        [void]$builder.Append($character)
    }

    return (Limit-Utf8ByteCount $builder.ToString().Trim() $script:MaxServiceInstanceNameBytes)
}


# ============================================================================
# Repair: transport settings
# ============================================================================

function Repair-TransportSettings($Section)
{
    if (-not $Section.Contains('transportSettings'))
    {
        return
    }

    $settings = $Section['transportSettings']

    if (-not (Test-JsonObjectNode $settings))
    {
        $Section.Remove('transportSettings')
        Add-Change 'transportSettings' 'Removed the section. It was not a JSON object.'
        return
    }

    Remove-UnknownKeys $settings $script:TransportSettingKeyOrder 'transportSettings'

    foreach ($key in @($script:TransportSettingBounds.Keys))
    {
        if (-not $settings.Contains($key))
        {
            continue
        }

        $bounds = $script:TransportSettingBounds[$key]
        $value  = $settings[$key]

        if ($value -is [bool] -or $value -is [string] -or $null -eq $value)
        {
            $settings.Remove($key)
            Add-Change 'transportSettings' "Removed '$key'. It was not a number, so the default of $($bounds.Default) applies."
            continue
        }

        $number = [double]$value

        if ([double]::IsNaN($number) -or [double]::IsInfinity($number) -or $number -ne [math]::Floor($number))
        {
            $settings.Remove($key)
            Add-Change 'transportSettings' "Removed '$key'. It was not a whole number, so the default of $($bounds.Default) applies."
            continue
        }

        if ($number -lt $bounds.Minimum -or $number -gt $bounds.Maximum)
        {
            $settings.Remove($key)
            Add-Change 'transportSettings' "Removed '$key'. $number is outside the supported range $($bounds.Minimum) to $($bounds.Maximum), so the default of $($bounds.Default) applies."
            continue
        }

        $settings[$key] = [int64]$number
    }

    foreach ($key in $script:ReservedTransportSettingKeys)
    {
        if ($settings.Contains($key))
        {
            Add-Warning 'transportSettings' "'$key' is reserved for a future release and has no effect today. It was left in place."
        }
    }

    if ($settings.Count -eq 0)
    {
        $Section.Remove('transportSettings')
        Add-Change 'transportSettings' 'Removed the now empty section.'
        return
    }

    Set-NodeKeyOrder $settings $script:TransportSettingKeyOrder
}


# ============================================================================
# Repair: allowed and denied client identity lists
# ============================================================================

# ReadRemoteClientIdentityList skips any element which cannot identify a device, so an entry
# missing either half of the pair is dead weight in the file.
function Repair-RemoteClientIdentityList($Host_, [string] $Key, [string] $Scope)
{
    if (-not $Host_.Contains($Key))
    {
        return @()
    }

    $list = $Host_[$Key]

    if (-not (Test-JsonArrayNode $list))
    {
        $Host_.Remove($Key)
        Add-Change $Scope "Removed '$Key'. It was not a JSON array."
        return @()
    }

    $repaired = New-JsonArrayNode
    $seen     = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)

    foreach ($element in $list)
    {
        if (-not (Test-JsonObjectNode $element))
        {
            Add-Change $Scope "Removed an entry from '$Key' which was not a JSON object."
            continue
        }

        $endpointName     = Get-StringValue $element 'umpEndpointName'
        $productInstanceId = Get-StringValue $element 'productInstanceId'

        if ([string]::IsNullOrWhiteSpace($endpointName) -or [string]::IsNullOrWhiteSpace($productInstanceId))
        {
            Add-Change $Scope "Removed an entry from '$Key' which was missing 'umpEndpointName' or 'productInstanceId'. Both are needed to identify a device."
            continue
        }

        # MidiNetworkRemoteClientIdentity::Key(), lowercased productInstanceId|umpEndpointName
        $identityKey = "$productInstanceId|$endpointName"

        if (-not $seen.Add($identityKey))
        {
            Add-Change $Scope "Removed a duplicate '$endpointName' entry from '$Key'."
            continue
        }

        $entry = New-JsonObjectNode
        $entry['umpEndpointName']   = $endpointName
        $entry['productInstanceId'] = $productInstanceId

        if ($element.Count -gt 2)
        {
            Add-Change $Scope "Removed unused properties from the '$endpointName' entry in '$Key'."
        }

        $repaired.Add($entry)
    }

    $Host_[$Key] = $repaired

    return @($seen)
}


# ============================================================================
# Repair: hosts
# ============================================================================

function Repair-HostEntry($HostEntry, [string] $Scope)
{
    Remove-UnknownKeys $HostEntry $script:HostKeyOrder $Scope

    # networkProtocol. UDP is the only protocol the transport supports, and a non-udp value
    # makes the whole entry fail with NETWORK_ERROR_CODE_INVALID_NETWORK_PROTOCOL.
    $protocol = Get-StringValue $HostEntry 'networkProtocol'

    if ($null -eq $protocol -or $protocol.ToLowerInvariant() -ne 'udp')
    {
        if ($null -ne $protocol -and $protocol -ne '')
        {
            Add-Change $Scope "Set 'networkProtocol' to ""udp"". ""$protocol"" is not supported."
        }

        $HostEntry['networkProtocol'] = 'udp'
    }
    elseif ($HostEntry['networkProtocol'] -cne 'udp')
    {
        $HostEntry['networkProtocol'] = 'udp'
        Add-Change $Scope "Normalized 'networkProtocol' to lower case."
    }

    # name. Required, and the whole entry is refused without it.
    Repair-TextProperty $HostEntry 'name' $script:MaxEndpointNameBytes $Scope

    if (-not $HostEntry.Contains('name'))
    {
        $generated = Limit-Utf8ByteCount "Windows $env:COMPUTERNAME" $script:MaxEndpointNameBytes

        $HostEntry['name'] = $generated
        Add-Change $Scope "Added the required 'name' as ""$generated""."
    }

    # productInstanceId. Optional. Left out, the service supplies a machine derived value, so a
    # bad one is better removed than kept.
    if ($HostEntry.Contains('productInstanceId'))
    {
        $productInstanceId = Get-StringValue $HostEntry 'productInstanceId'

        if ([string]::IsNullOrWhiteSpace($productInstanceId))
        {
            $HostEntry.Remove('productInstanceId')
            Add-Change $Scope "Removed the empty 'productInstanceId'. The service supplies one."
        }
        elseif ((Get-Utf8ByteCount $productInstanceId) -gt $script:MaxProductInstanceIdBytes)
        {
            $HostEntry.Remove('productInstanceId')
            Add-Change $Scope "Removed 'productInstanceId'. It was longer than the $($script:MaxProductInstanceIdBytes) byte limit in the MIDI 2.0 specification, so the service supplies one."
        }
        elseif (-not (Test-PrintableAscii $productInstanceId))
        {
            $HostEntry.Remove('productInstanceId')
            Add-Change $Scope "Removed 'productInstanceId'. Only printable ASCII is allowed, so the service supplies one."
        }
        else
        {
            $HostEntry['productInstanceId'] = $productInstanceId
        }
    }

    # serviceInstanceName. Becomes the DNS-SD label and the virtual parent device id.
    if ($HostEntry.Contains('serviceInstanceName'))
    {
        $serviceInstanceName = Get-StringValue $HostEntry 'serviceInstanceName'

        if ($null -eq $serviceInstanceName)
        {
            $HostEntry.Remove('serviceInstanceName')
            Add-Change $Scope "Removed 'serviceInstanceName'. It was not a string, so the machine name is used."
        }
        else
        {
            $compliant = Get-CompliantServiceInstanceName $serviceInstanceName

            if ($compliant -eq '')
            {
                $HostEntry.Remove('serviceInstanceName')
                Add-Change $Scope "Removed 'serviceInstanceName'. Nothing usable was left after removing characters a DNS-SD label cannot carry."
            }
            elseif ($compliant -cne $HostEntry['serviceInstanceName'])
            {
                $HostEntry['serviceInstanceName'] = $compliant
                Add-Change $Scope "Corrected 'serviceInstanceName' to ""$compliant"". A DNS-SD instance label is a single label, so periods and control characters are not allowed and the limit is $($script:MaxServiceInstanceNameBytes) UTF-8 bytes."
            }
        }
    }

    # port. SafeGetNamedString refuses a JSON number, so a port written as one is read as an
    # empty string and the host silently falls back to an automatically allocated port.
    $originalPort = if ($HostEntry.Contains('port')) { $HostEntry['port'] } else { $null }
    $portText     = if ($HostEntry.Contains('port')) { Get-NormalizedPortText $originalPort } else { 'auto' }

    if ($null -eq $portText)
    {
        Add-Change $Scope "Set 'port' to ""auto"". ""$originalPort"" is not a port number between 1 and 65535."
        $portText = 'auto'
    }
    elseif ($null -ne $originalPort -and $originalPort -isnot [string])
    {
        Add-Change $Scope "Wrote 'port' as the string ""$portText"". The transport reads it as a string and ignores a JSON number."
    }

    $HostEntry['port'] = $portText

    # allowPortFallback only means anything alongside a specific port
    Repair-BooleanProperty $HostEntry 'allowPortFallback' $Scope

    if ($portText -eq 'auto' -and $HostEntry.Contains('allowPortFallback'))
    {
        $HostEntry.Remove('allowPortFallback')
        Add-Change $Scope "Removed 'allowPortFallback'. It only applies when a specific port is configured."
    }

    Repair-BooleanProperty $HostEntry 'enabled' $Scope
    Repair-BooleanProperty $HostEntry 'advertise' $Scope
    Repair-BooleanProperty $HostEntry 'createMidi1Ports' $Scope

    Repair-TextProperty $HostEntry 'customEndpointName' $script:MaxEndpointNameBytes $Scope

    # authentication
    $authentication = Get-StringValue $HostEntry 'authentication'

    if ($HostEntry.Contains('authentication'))
    {
        $normalized = $null

        if ($null -ne $authentication)
        {
            switch ($authentication.ToLowerInvariant())
            {
                'none'     { $normalized = 'none' }
                'password' { $normalized = 'password' }
                'user'     { $normalized = 'user' }
            }
        }

        if ($null -eq $normalized)
        {
            $HostEntry.Remove('authentication')
            Add-Change $Scope "Removed 'authentication'. ""$authentication"" is not one of none, password or user, so no authentication is used."
            $authentication = 'none'
        }
        else
        {
            if ($normalized -cne $HostEntry['authentication'])
            {
                $HostEntry['authentication'] = $normalized
                Add-Change $Scope "Normalized 'authentication' to ""$normalized""."
            }

            $authentication = $normalized
        }
    }
    else
    {
        $authentication = 'none'
    }

    if ($authentication -eq 'none')
    {
        foreach ($credentialKey in @('globalPassword', 'userAuth'))
        {
            if ($HostEntry.Contains($credentialKey))
            {
                $HostEntry.Remove($credentialKey)
                Add-Change $Scope "Removed '$credentialKey'. It only applies to a host configured for authentication."
            }
        }
    }
    else
    {
        # ValidateHostDefinition returns E_NOTIMPL for any authenticated host, so this entry can
        # never start. Disabling it is explicit and reversible, and it is fail closed: turning
        # authentication off instead would put an unprotected host on the network.
        if ($HostEntry['enabled'] -ne $false)
        {
            $HostEntry['enabled'] = $false
            Add-Change $Scope "Set 'enabled' to false. Network MIDI 2.0 authentication is not implemented yet, so this host is refused by the service."
        }

        Add-Warning $Scope "This host asks for ""$authentication"" authentication, which no current build supports. It was disabled rather than downgraded to no authentication."
    }

    # remoteClientPolicy
    if ($HostEntry.Contains('remoteClientPolicy'))
    {
        $policy     = Get-StringValue $HostEntry 'remoteClientPolicy'
        $normalized = $null

        if ($null -ne $policy)
        {
            switch ($policy.ToLowerInvariant())
            {
                'allowany'        { $normalized = 'allowAny' }
                'requireapproval' { $normalized = 'requireApproval' }
            }
        }

        if ($null -eq $normalized)
        {
            $HostEntry.Remove('remoteClientPolicy')
            Add-Change $Scope "Removed 'remoteClientPolicy'. ""$policy"" is not one of allowAny or requireApproval, so allowAny is used."
        }
        elseif ($normalized -cne $HostEntry['remoteClientPolicy'])
        {
            $HostEntry['remoteClientPolicy'] = $normalized
            Add-Change $Scope "Normalized 'remoteClientPolicy' to ""$normalized""."
        }
    }

    # Wrapped, because PowerShell unrolls a returned array and a single identity would otherwise
    # arrive here as a bare string
    $allowedKeys = @(Repair-RemoteClientIdentityList $HostEntry 'allowedClients' $Scope)
    $deniedKeys  = @(Repair-RemoteClientIdentityList $HostEntry 'deniedClients'  $Scope)

    # MidiNetworkHost::EvaluateRemoteClient checks the deny list first, so an identity in both
    # lists is denied. Leaving it in the allow list only misleads whoever reads the file.
    if ($allowedKeys.Count -gt 0 -and $deniedKeys.Count -gt 0)
    {
        $denied = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)

        foreach ($key in $deniedKeys)
        {
            [void]$denied.Add($key)
        }

        $filtered = New-JsonArrayNode

        foreach ($entry in $HostEntry['allowedClients'])
        {
            $identityKey = "$($entry['productInstanceId'])|$($entry['umpEndpointName'])"

            if ($denied.Contains($identityKey))
            {
                Add-Change $Scope "Removed ""$($entry['umpEndpointName'])"" from 'allowedClients'. It is also in 'deniedClients', and the deny list wins."
                continue
            }

            $filtered.Add($entry)
        }

        $HostEntry['allowedClients'] = $filtered
    }

    Set-NodeKeyOrder $HostEntry $script:HostKeyOrder

    return $portText
}


# ============================================================================
# Repair: clients
# ============================================================================

function Repair-ClientEntry($ClientEntry, [string] $Scope)
{
    Remove-UnknownKeys $ClientEntry $script:ClientKeyOrder $Scope

    $protocol = Get-StringValue $ClientEntry 'networkProtocol'

    if ($null -eq $protocol -or $protocol.ToLowerInvariant() -ne 'udp')
    {
        if ($null -ne $protocol -and $protocol -ne '')
        {
            Add-Change $Scope "Set 'networkProtocol' to ""udp"". ""$protocol"" is not supported."
        }

        $ClientEntry['networkProtocol'] = 'udp'
    }
    elseif ($ClientEntry['networkProtocol'] -cne 'udp')
    {
        $ClientEntry['networkProtocol'] = 'udp'
        Add-Change $Scope "Normalized 'networkProtocol' to lower case."
    }

    Repair-BooleanProperty $ClientEntry 'enabled' $Scope
    Repair-BooleanProperty $ClientEntry 'createMidi1Ports' $Scope

    Repair-TextProperty $ClientEntry 'customEndpointName' $script:MaxEndpointNameBytes $Scope

    if (-not $ClientEntry.Contains('match') -or -not (Test-JsonObjectNode $ClientEntry['match']))
    {
        Add-Change $Scope "Removed the entry. Without a 'match' object there is no way to find the remote host, and the service reports NETWORK_ERROR_CODE_MISSING_MATCH_ENTRY."
        return $false
    }

    $match = $ClientEntry['match']

    Remove-UnknownKeys $match $script:MatchKeyOrder "$Scope match"

    foreach ($key in @('id', 'umpEndpointName', 'umpProductInstanceId', 'directHostNameOrIP', 'serviceInstance'))
    {
        if (-not $match.Contains($key))
        {
            continue
        }

        $value = Get-StringValue $match $key

        if ([string]::IsNullOrWhiteSpace($value))
        {
            $match.Remove($key)
            Add-Change "$Scope match" "Removed the empty '$key'."
        }
        else
        {
            $match[$key] = $value
        }
    }

    if ($match.Contains('serviceInstance'))
    {
        Add-Warning "$Scope match" "'serviceInstance' is declared but is not read by the transport. Use 'id', 'umpEndpointName' or 'umpProductInstanceId' instead."
    }

    # directPort
    if ($match.Contains('directPort'))
    {
        $originalDirectPort = $match['directPort']
        $directPort         = Get-NormalizedPortText $originalDirectPort

        if ($null -eq $directPort -or $directPort -eq 'auto')
        {
            $match.Remove('directPort')
            Add-Change "$Scope match" "Removed 'directPort'. It was not a port number between 1 and 65535."
        }
        elseif ($originalDirectPort -isnot [string] -or $originalDirectPort -cne $directPort)
        {
            $match['directPort'] = $directPort
            Add-Change "$Scope match" "Wrote 'directPort' as the string ""$directPort"". The transport reads it as a string and ignores a JSON number."
        }
    }

    # A direct connection needs both halves. With only one, the entry can still work if it has an
    # mDNS criterion.
    if ($match.Contains('directHostNameOrIP') -and -not $match.Contains('directPort'))
    {
        $match.Remove('directHostNameOrIP')
        Add-Change "$Scope match" "Removed 'directHostNameOrIP'. A direct connection also needs 'directPort'."
    }
    elseif ($match.Contains('directPort') -and -not $match.Contains('directHostNameOrIP'))
    {
        $match.Remove('directPort')
        Add-Change "$Scope match" "Removed 'directPort'. A direct connection also needs 'directHostNameOrIP'."
    }

    $usable = @($script:UsableMatchKeys | Where-Object { $match.Contains($_) })

    if ($usable.Count -eq 0)
    {
        Add-Change $Scope "Removed the entry. Its 'match' object has no criterion the transport can use, so it could never connect."
        return $false
    }

    Set-NodeKeyOrder $match $script:MatchKeyOrder
    Set-NodeKeyOrder $ClientEntry $script:ClientKeyOrder

    return $true
}


# ============================================================================
# Repair: the keyed hosts and clients collections
# ============================================================================

function Repair-EntryCollection($CreateSection, [string] $CollectionKey)
{
    if (-not $CreateSection.Contains($CollectionKey))
    {
        return
    }

    $collection = $CreateSection[$CollectionKey]

    if (-not (Test-JsonObjectNode $collection))
    {
        $CreateSection.Remove($CollectionKey)
        Add-Change "create.$CollectionKey" 'Removed the collection. It was not a JSON object.'
        return
    }

    $repaired = New-JsonObjectNode

    # host service instance names and explicit ports have to be unique across hosts, or the
    # second one is refused with SERVICE_INSTANCE_NAME_IN_USE or HOST_PORT_IN_USE
    $usedServiceInstanceNames = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
    $usedPorts                = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::Ordinal)

    foreach ($originalKey in @($collection.Keys))
    {
        $entry = $collection[$originalKey]

        # Every message for one entry is reported against the key it has in the file, so a
        # renamed entry does not appear twice in the report under two different names.
        $scope = "create.$CollectionKey[$originalKey]"

        if (-not (Test-JsonObjectNode $entry))
        {
            Add-Change $scope 'Removed the entry. Its value was not a JSON object.'
            continue
        }

        if ($CollectionKey -eq 'hosts')
        {
            $port = Repair-HostEntry $entry $scope

            # Absent means the machine name, which two hosts would then share
            $serviceInstanceName = Get-StringValue $entry 'serviceInstanceName'

            if ([string]::IsNullOrWhiteSpace($serviceInstanceName))
            {
                $serviceInstanceName = Get-CompliantServiceInstanceName $env:COMPUTERNAME
            }

            if (-not $usedServiceInstanceNames.Add($serviceInstanceName))
            {
                $suffix    = 2
                $candidate = $serviceInstanceName

                while (-not $usedServiceInstanceNames.Add($candidate) -and $suffix -le 99)
                {
                    $decoration = '-{0:00}' -f $suffix
                    $room       = $script:MaxServiceInstanceNameBytes - $decoration.Length
                    $candidate  = (Limit-Utf8ByteCount $serviceInstanceName $room) + $decoration
                    $suffix++
                }

                $entry['serviceInstanceName'] = $candidate
                Add-Change $scope "Set 'serviceInstanceName' to ""$candidate"". Another host already uses ""$serviceInstanceName"", and the name becomes both the DNS-SD label and the virtual parent device id."
            }

            if ($port -ne 'auto' -and -not $usedPorts.Add($port))
            {
                $entry['port'] = 'auto'
                $entry.Remove('allowPortFallback')
                Add-Change $scope "Set 'port' to ""auto"". Another host is already configured for port $port, and two hosts cannot share one."
            }
        }
        elseif (-not (Repair-ClientEntry $entry $scope))
        {
            continue
        }

        # The service parses the key with winrt::guid and rejects the entry when that fails.
        # Reported only now, because an entry which was dropped above was never worth renaming.
        $canonicalKey = Get-CanonicalGuidKey $originalKey

        if ($null -eq $canonicalKey)
        {
            $canonicalKey = [guid]::NewGuid().ToString('B').ToLowerInvariant()
            Add-Change $scope "Replaced the entry identifier with the new GUID $canonicalKey. Identifiers must be GUIDs."
        }
        elseif ($canonicalKey -cne $originalKey)
        {
            Add-Change $scope "Rewrote the entry identifier as $canonicalKey, the braced lower case form the API writes."
        }

        if ($repaired.Contains($canonicalKey))
        {
            $replacement = [guid]::NewGuid().ToString('B').ToLowerInvariant()
            Add-Change $scope "Another entry already resolved to the identifier $canonicalKey. This one was given the new GUID $replacement."
            $canonicalKey = $replacement
        }

        $repaired[$canonicalKey] = $entry
    }

    if ($repaired.Count -eq 0)
    {
        $CreateSection.Remove($CollectionKey)
        Add-Change "create.$CollectionKey" 'Removed the now empty collection.'
        return
    }

    $CreateSection[$CollectionKey] = $repaired
}


# ============================================================================
# Repair: the whole Network MIDI 2.0 section
# ============================================================================

function Repair-NetworkSection($Section)
{
    # "remove" is the shape of a live removal command. MergeTransportSection applies one and
    # never writes it back, so a copy in the file is a leftover from a hand edit.
    if ($Section.Contains('remove'))
    {
        $Section.Remove('remove')
        Add-Change 'section' "Removed the 'remove' block. It is a command shape which the service applies and never stores."
    }

    Remove-UnknownKeys $Section $script:SectionKeyOrder 'section'

    if ($Section.Contains('update') -and -not (Test-JsonArrayNode $Section['update']))
    {
        $Section.Remove('update')
        Add-Change 'section' "Removed 'update'. Endpoint customizations are read as a JSON array of match and customProperties pairs."
    }

    Repair-TransportSettings $Section

    if ($Section.Contains('create'))
    {
        $createSection = $Section['create']

        if (-not (Test-JsonObjectNode $createSection))
        {
            $Section.Remove('create')
            Add-Change 'create' 'Removed the section. It was not a JSON object.'
        }
        else
        {
            Remove-UnknownKeys $createSection @('hosts', 'clients') 'create'

            Repair-EntryCollection $createSection 'hosts'
            Repair-EntryCollection $createSection 'clients'

            if ($createSection.Count -eq 0)
            {
                $Section.Remove('create')
                Add-Change 'create' 'Removed the now empty section.'
            }
            else
            {
                Set-NodeKeyOrder $createSection @('hosts', 'clients')
            }
        }
    }

    Add-SectionComment $Section

    Set-NodeKeyOrder $Section $script:SectionKeyOrder
}

function Add-SectionComment($Section)
{
    $comment = Get-StringValue $Section '_comment'

    if ([string]::IsNullOrWhiteSpace($comment))
    {
        $Section['_comment'] = $script:NetworkTransportComment
        Add-Change 'section' "Added the '_comment' describing the transport."
    }
}


# ============================================================================
# Main
# ============================================================================

Write-Banner 'Windows MIDI Services : Network MIDI 2.0 configuration repair'

Write-Host 'This tool inspects the Network MIDI 2.0 (UDP) entries in your Windows MIDI'   -ForegroundColor Gray
Write-Host 'Services configuration file, and either repairs them or removes them all.'    -ForegroundColor Gray
Write-Host ''
Write-Host 'The service skips a configuration entry it cannot accept, and does so'        -ForegroundColor Gray
Write-Host 'silently, so a host or a connection you configured simply never appears.'     -ForegroundColor Gray
Write-Host 'Older previews of the API also wrote properties which are no longer read,'    -ForegroundColor Gray
Write-Host 'such as the IP address policy lists which were replaced by remote client'     -ForegroundColor Gray
Write-Host 'approval.'                                                                    -ForegroundColor Gray
Write-Host ''
Write-Host 'Repairing will:'                                                              -ForegroundColor Gray
Write-Host '  - make every host and connection identifier a GUID, in the form the API'    -ForegroundColor Gray
Write-Host '    writes'                                                                   -ForegroundColor Gray
Write-Host '  - remove properties from older versions of the API, and anything the'       -ForegroundColor Gray
Write-Host '    transport does not read'                                                  -ForegroundColor Gray
Write-Host '  - remove values outside the range the service allows, so its own default'   -ForegroundColor Gray
Write-Host '    is used instead'                                                          -ForegroundColor Gray
Write-Host '  - remove entries which can never work, such as a connection with nothing'   -ForegroundColor Gray
Write-Host '    to match a remote host against'                                           -ForegroundColor Gray
Write-Host ''
Write-Host 'Nothing outside the Network MIDI 2.0 (UDP) section is changed. The file is'   -ForegroundColor Gray
Write-Host 'reformatted, and a backup is taken first.'                                    -ForegroundColor Gray
Write-Host ''
Write-Host "Configuration file : $ConfigFilePath" -ForegroundColor Cyan
Write-Host ''

if (-not (Test-Path -LiteralPath $ConfigFilePath -PathType Leaf))
{
    Write-Host "That file does not exist. Use -ConfigFilePath to point at another one." -ForegroundColor Red
    Write-Host ''
    exit 1
}

if ($ReportOnly)
{
    Write-Host 'Running in report only mode. Nothing will be written.' -ForegroundColor Yellow
    Write-Host ''
}

if (-not $Force -and -not $ReportOnly)
{
    if (-not (Read-YesNo 'Do you want to continue?'))
    {
        Write-Host ''
        Write-Host 'No changes were made.' -ForegroundColor Yellow
        Write-Host ''
        exit 0
    }
}

# Read and parse before backing up, so a file which cannot be parsed does not leave a backup
# behind for no reason.
$originalText = [System.IO.File]::ReadAllText($ConfigFilePath, [System.Text.Encoding]::UTF8)

try
{
    $parsed = $originalText | ConvertFrom-Json
}
catch
{
    Write-Host ''

    # PowerShell's own parser refuses two keys which differ only in case, even though JSON
    # allows them and the service treats them as two separate entries.
    if ($_.Exception.Message -like '*different casing*')
    {
        Write-Host 'That file contains two JSON keys which differ only in letter case, which this' -ForegroundColor Red
        Write-Host 'script cannot read. Two such keys are separate entries to the service, so this' -ForegroundColor Red
        Write-Host 'is worth correcting by hand.' -ForegroundColor Red
        Write-Host ''
        Write-Host 'Open the file, delete or rename one of the two keys named below, then run this' -ForegroundColor Yellow
        Write-Host 'script again.' -ForegroundColor Yellow
    }
    else
    {
        Write-Host 'That file could not be parsed, so it cannot be repaired automatically.' -ForegroundColor Red
    }

    Write-Host ''
    Write-Host $_.Exception.Message -ForegroundColor Gray
    Write-Host ''
    exit 1
}

$root = ConvertTo-JsonNode -Value $parsed

if (-not (Test-JsonObjectNode $root))
{
    Write-Host ''
    Write-Host 'The root of that file is not a JSON object, so it is not a MIDI configuration file.' -ForegroundColor Red
    Write-Host ''
    exit 1
}

# Locate the transport section. GUID keys are compared as GUIDs rather than as text, because the
# file may hold any of the forms winrt::guid accepts.
$pluginSettings = $null
$networkKey     = $null

if ($root.Contains($script:TransportPluginSettingsKey))
{
    $candidate = $root[$script:TransportPluginSettingsKey]

    if (Test-JsonObjectNode $candidate)
    {
        $pluginSettings = $candidate

        $wanted = Get-CanonicalGuidKey $script:NetworkTransportId

        foreach ($key in @($pluginSettings.Keys))
        {
            if ((Get-CanonicalGuidKey $key) -eq $wanted)
            {
                $networkKey = $key
                break
            }
        }
    }
}

if ($null -eq $pluginSettings)
{
    Write-Host ''
    Write-Host "That file has no '$($script:TransportPluginSettingsKey)' object, so there is nothing to repair." -ForegroundColor Yellow
    Write-Host ''
    exit 0
}

if ($null -eq $networkKey)
{
    Write-Host ''
    Write-Host 'That file has no Network MIDI 2.0 (UDP) section, so there is nothing to repair.' -ForegroundColor Yellow
    Write-Host ''
    exit 0
}

$networkSection = $pluginSettings[$networkKey]

if (-not (Test-JsonObjectNode $networkSection))
{
    $networkSection = New-JsonObjectNode
    Add-Change 'section' 'Replaced the Network MIDI 2.0 section. It was not a JSON object.'
}

# Back up
$backupPath = $null

if (-not $ReportOnly)
{
    $stamp      = (Get-Date).ToString('yyyy-MM-dd-HHmmss')
    $backupPath = "$ConfigFilePath.$stamp.network-repair.bak"

    try
    {
        # Written rather than copied, because the configuration folder does not always grant
        # delete permission and a plain write always works where a replace may not.
        [System.IO.File]::WriteAllText($backupPath, $originalText, (New-Object System.Text.UTF8Encoding($false)))
    }
    catch
    {
        Write-Host ''
        Write-Host "Unable to write the backup to $backupPath" -ForegroundColor Red
        Write-Host $_.Exception.Message -ForegroundColor Red
        Write-Host ''
        Write-Host 'No changes were made. Try again from an elevated prompt.' -ForegroundColor Yellow
        Write-Host ''
        exit 1
    }

    Write-Host ''
    Write-Host "Backup written to $backupPath" -ForegroundColor Green
}

# Choose what to do
$chosenAction = $Action

if ($chosenAction -eq 'Ask')
{
    Write-Host ''
    Write-Rule
    Write-Host 'What would you like to do with the Network MIDI 2.0 (UDP) entries?' -ForegroundColor DarkCyan
    Write-Rule
    Write-Host '  [R] Repair  - correct what can be corrected, and drop what cannot' -ForegroundColor Gray
    Write-Host '  [X] Remove  - delete every Network MIDI 2.0 host, connection and'  -ForegroundColor Gray
    Write-Host '                setting, along with any endpoint customizations for' -ForegroundColor Gray
    Write-Host '                network endpoints, leaving an empty section'         -ForegroundColor Gray
    Write-Host '  [C] Cancel'                                                        -ForegroundColor Gray
    Write-Host ''

    while ($chosenAction -eq 'Ask')
    {
        $answer = (Read-Host 'Choose R, X or C').Trim().ToLowerInvariant()

        if ($answer -eq 'r' -or $answer -eq 'repair') { $chosenAction = 'Repair' }
        elseif ($answer -eq 'x' -or $answer -eq 'remove') { $chosenAction = 'RemoveAll' }
        elseif ($answer -eq 'c' -or $answer -eq 'cancel')
        {
            Write-Host ''
            Write-Host 'No changes were made.' -ForegroundColor Yellow

            if ($null -ne $backupPath)
            {
                Write-Host "The backup at $backupPath was left in place." -ForegroundColor Gray
            }

            Write-Host ''
            exit 0
        }
        else
        {
            Write-Host 'Please answer R, X or C.' -ForegroundColor Yellow
        }
    }
}

if ($chosenAction -eq 'RemoveAll')
{
    if (-not $Force -and -not $ReportOnly)
    {
        Write-Host ''
        Write-Host 'Every Network MIDI 2.0 host, connection, transport setting and endpoint'  -ForegroundColor Yellow
        Write-Host 'customization in this file will be deleted.'                              -ForegroundColor Yellow
        Write-Host ''

        if (-not (Read-YesNo 'Are you sure?'))
        {
            Write-Host ''
            Write-Host 'No changes were made.' -ForegroundColor Yellow
            Write-Host ''
            exit 0
        }
    }

    $existingComment = Get-StringValue $networkSection '_comment'

    $replacement = New-JsonObjectNode
    $replacement['_comment'] = if ([string]::IsNullOrWhiteSpace($existingComment)) { $script:NetworkTransportComment } else { $existingComment }

    $removedHosts   = 0
    $removedClients = 0

    if ($networkSection.Contains('create') -and (Test-JsonObjectNode $networkSection['create']))
    {
        $createSection = $networkSection['create']

        if ($createSection.Contains('hosts') -and (Test-JsonObjectNode $createSection['hosts']))
        {
            $removedHosts = $createSection['hosts'].Count
        }

        if ($createSection.Contains('clients') -and (Test-JsonObjectNode $createSection['clients']))
        {
            $removedClients = $createSection['clients'].Count
        }
    }

    Add-Change 'section' "Removed $removedHosts host entries and $removedClients connection entries."
    Add-Change 'section' 'Removed the transport settings, endpoint customizations and everything else, leaving a comment only section.'

    $networkSection = $replacement
}
else
{
    Repair-NetworkSection $networkSection
}

$pluginSettings[$networkKey] = $networkSection

# Report
Write-Host ''
Write-Rule
Write-Host 'Results' -ForegroundColor DarkCyan
Write-Rule

if ($script:Changes.Count -eq 0)
{
    Write-Host 'No changes were needed. The Network MIDI 2.0 section is already valid.' -ForegroundColor Green
}
else
{
    foreach ($group in ($script:Changes | Group-Object Scope))
    {
        Write-Host ''
        Write-Host $group.Name -ForegroundColor Cyan

        foreach ($change in $group.Group)
        {
            Write-Host "  - $($change.Message)" -ForegroundColor Gray
        }
    }
}

if ($script:Warnings.Count -gt 0)
{
    Write-Host ''
    Write-Rule
    Write-Host 'Worth knowing' -ForegroundColor DarkCyan
    Write-Rule

    foreach ($group in ($script:Warnings | Group-Object Scope))
    {
        Write-Host ''
        Write-Host $group.Name -ForegroundColor Cyan

        foreach ($warning in $group.Group)
        {
            Write-Host "  - $($warning.Message)" -ForegroundColor Yellow
        }
    }
}

Write-Host ''

if ($ReportOnly)
{
    Write-Host 'Report only mode. Nothing was written.' -ForegroundColor Yellow
    Write-Host ''
    exit 0
}

if ($script:Changes.Count -eq 0)
{
    Write-Host 'Nothing to write.' -ForegroundColor Green

    if ($null -ne $backupPath)
    {
        Write-Host "The backup at $backupPath was left in place." -ForegroundColor Gray
    }

    Write-Host ''
    exit 0
}

# Same shape the SDK's own writer produces: four space indent, UTF-8 without a BOM, and a
# trailing newline.
$newText = (ConvertTo-JsonText -Node $root -Depth 0) + "`n"

# Parsing what we are about to write catches a serializer mistake before it reaches the file
try
{
    [void]($newText | ConvertFrom-Json)
}
catch
{
    Write-Host 'The repaired configuration did not parse back, so nothing was written.' -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    Write-Host ''
    exit 1
}

if (-not $Force)
{
    if (-not (Read-YesNo 'Write these changes to the configuration file?' $true))
    {
        Write-Host ''
        Write-Host 'No changes were made.' -ForegroundColor Yellow
        Write-Host ''
        exit 0
    }
}

try
{
    # In place, never delete and recreate. The configuration folder grants write but not always
    # delete, so a replace via rename can fail where a plain write succeeds.
    [System.IO.File]::WriteAllText($ConfigFilePath, $newText, (New-Object System.Text.UTF8Encoding($false)))
}
catch
{
    Write-Host ''
    Write-Host "Unable to write $ConfigFilePath" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    Write-Host ''
    Write-Host "The original file is unchanged, and a copy of it is at $backupPath" -ForegroundColor Yellow
    Write-Host ''
    exit 1
}

Write-Host ''
Write-Host "Wrote $ConfigFilePath" -ForegroundColor Green
Write-Host "Backup at $backupPath" -ForegroundColor Gray
Write-Host ''
Write-Host 'The MIDI service reads this file when it starts, so restart it to pick up the' -ForegroundColor Yellow
Write-Host 'changes. From an elevated prompt:' -ForegroundColor Yellow
Write-Host '    net stop midisrv' -ForegroundColor White
Write-Host '    net start midisrv' -ForegroundColor White
Write-Host ''
Write-Host 'Anything removed here is still live in the running service until it restarts.' -ForegroundColor Gray
Write-Host ''

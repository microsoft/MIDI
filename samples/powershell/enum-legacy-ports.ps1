#Requires -Version 7.6
import-module WindowsMidiServices

# Confirms Windows MIDI Services is available before anything else is attempted.
Start-Midi

# These are the MIDI 1.0 ports that older applications see through WinMM and
# WinRT MIDI 1.0. If you are porting a script or an application which thinks in
# WinMM port numbers, this is the view you already know.
#
# Each port maps to one group on a UMP endpoint. The AssociatedEndpointDeviceId
# is how you get from the old view to the new one.

Write-Host "All MIDI 1.0 ports" -ForegroundColor Cyan
Get-MidiLegacyPort | Sort-Object -Property Name | Format-Table -AutoSize

# Flow separates inputs from outputs. Under WinMM these were two entirely
# separate numbering spaces, which is why port 3 could mean two different things.
Write-Host "Sources only (MIDI inputs)" -ForegroundColor Cyan
Get-MidiLegacyPort -Flow MidiMessageSource | Sort-Object -Property Name | Format-Table -AutoSize

Write-Host "Destinations only (MIDI outputs)" -ForegroundColor Cyan
Get-MidiLegacyPort -Flow MidiMessageDestination | Sort-Object -Property Name | Format-Table -AutoSize

# You can also go the other way: given an endpoint, find the MIDI 1.0 ports it
# produced. This is the useful direction when you have a modern device and want
# to tell a user which old port names it will appear under.
Write-Host "Ports grouped by owning endpoint" -ForegroundColor Cyan
Write-Host ""

foreach ($endpoint in (Get-MidiEndpointDeviceInfo | Sort-Object -Property Name))
{
    $ports = Get-MidiLegacyPort -EndpointDeviceId $endpoint.EndpointDeviceId

    if ($null -ne $ports)
    {
        Write-Host $endpoint.Name -ForegroundColor Yellow
        $ports | Format-Table -AutoSize
    }
}

# The Number property is the current WinMM port number. It can change when
# devices are added or removed, so do not save it. Save the PortDeviceId, or
# better, the endpoint device id and group.
Write-Host "Note: the Number column is the current WinMM port number and is not stable." -ForegroundColor DarkGray

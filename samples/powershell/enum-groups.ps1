#Requires -Version 7.6
import-module WindowsMidiServices

# Confirms Windows MIDI Services is available before anything else is attempted.
Start-Midi

# Groups are the closest thing to a "port" in Windows MIDI Services. A single
# endpoint carries up to 16 groups in each direction, so where WinMM would have
# shown you several unrelated ports, you get one device with several groups.
#
# The names come from function blocks when the device declares them, and from
# group terminal blocks otherwise. Function blocks win when both are present.

Write-Host "Endpoints and their groups" -ForegroundColor Cyan
Write-Host ""

foreach ($endpoint in (Get-MidiEndpointDeviceInfo | Sort-Object -Property Name))
{
    Write-Host $endpoint.Name -ForegroundColor Yellow

    $groups = Get-MidiEndpointGroup $endpoint.EndpointDeviceId

    if ($null -ne $groups)
    {
        $groups | Format-Table -AutoSize
    }
    else
    {
        Write-Host "  No active groups reported." -ForegroundColor DarkGray
        Write-Host ""
    }
}

# Groups which are not currently active are hidden by default. Include them when
# you want to see everything the device could theoretically use.
Write-Host "Use -IncludeInactive to also list groups which are not currently active." -ForegroundColor DarkGray

#Requires -Version 7.6
import-module WindowsMidiServices

# Confirms Windows MIDI Services is available before anything else is attempted.
Start-Midi

# The simpler cousin of loopback-endpoints.ps1. Instead of a pair of MIDI 2.0
# endpoints wired to each other, this creates a single endpoint which behaves the
# way a MIDI 1.0 virtual cable does, and which shows up in the MIDI 1.0 port list
# that WinMM and WinRT MIDI 1.0 applications see.
#
# Use this one when you want something an existing application can talk to
# without any changes.
#
# Without -SaveToConfiguration the endpoint is TRANSIENT and disappears when the
# service stops.

Write-Host "Existing MIDI 1.0 style loopback endpoints" -ForegroundColor Cyan
Get-MidiBasicLoopback | Format-Table -AutoSize

Write-Host "Creating a transient basic loopback" -ForegroundColor Cyan

$loopback = New-MidiBasicLoopback "PowerShell Sample Basic Loopback" `
    -Description "Created by the PowerShell loopback-basic-endpoints sample." `
    -UniqueId "ps-sample-basic-loopback"

if ($null -ne $loopback)
{
    $loopback | Format-List

    $associationId = $loopback.AssociationId

    # Because this is a MIDI 1.0 style endpoint, the service also creates the
    # matching MIDI 1.0 ports for it. This is what an older application will see.
    Write-Host "MIDI 1.0 ports created for this endpoint" -ForegroundColor Cyan
    Get-MidiLegacyPort -EndpointDeviceId $loopback.EndpointDeviceId | Format-Table -AutoSize

    Write-Host "Muting the loopback" -ForegroundColor Cyan
    Set-MidiBasicLoopbackMute -AssociationId $associationId -Muted $true

    Start-Sleep -Seconds 2

    Write-Host "Unmuting the loopback" -ForegroundColor Cyan
    Set-MidiBasicLoopbackMute -AssociationId $associationId -Muted $false

    Write-Host ""
    Read-Host "Press Enter to remove the loopback endpoint"

    Write-Host "Removing the loopback" -ForegroundColor Cyan
    Remove-MidiBasicLoopback -AssociationId $associationId
}
else
{
    Write-Host "Unable to create the loopback endpoint." -ForegroundColor Red
    Write-Host "This happens if a previous run was killed before removing it." -ForegroundColor Red
    Write-Host "Restart the MIDI service, or change the unique id in this script." -ForegroundColor Red
}

#Requires -Version 7.6
import-module WindowsMidiServices

# Confirms Windows MIDI Services is available before anything else is attempted.
Start-Midi

# Creating loopback endpoints at runtime. A loopback is a pair of endpoints wired
# back to back: anything sent to A arrives at B, and the other way round.
#
# There was no WinMM equivalent, which is why Windows musicians historically had
# to install a third-party virtual cable driver.
#
# Without -SaveToConfiguration these endpoints are TRANSIENT: they exist only
# while the service is running, and nothing is written to the configuration
# file. That is what you want for a script. Add -SaveToConfiguration only when
# you intend the endpoints to come back after a reboot.

Write-Host "Existing MIDI 2.0 loopback endpoint pairs" -ForegroundColor Cyan
Get-MidiLoopback | Format-Table -AutoSize

# BaseName gives you "<name> A" and "<name> B" automatically. Use -NameA and
# -NameB instead when you want to name each side yourself.
Write-Host "Creating a transient loopback pair" -ForegroundColor Cyan

$loopback = New-MidiLoopback "PowerShell Sample Loopback" `
    -Description "Created by the PowerShell loopback-endpoints sample." `
    -UniqueId "ps-sample-loopback"

if ($null -ne $loopback)
{
    $loopback | Format-List

    $associationId = $loopback.AssociationId

    Write-Host "Both endpoints are now visible to every MIDI application on this PC." -ForegroundColor Green
    Write-Host ""

    # Muting stops messages flowing between the two sides without removing the
    # endpoints, so applications keep their connections open.
    Write-Host "Muting the loopback" -ForegroundColor Cyan
    Set-MidiLoopbackMute -AssociationId $associationId -Muted $true

    Start-Sleep -Seconds 2

    Write-Host "Unmuting the loopback" -ForegroundColor Cyan
    Set-MidiLoopbackMute -AssociationId $associationId -Muted $false

    Write-Host ""
    Read-Host "Press Enter to remove the loopback endpoints"

    # Remove what you created. If this script is killed before it gets here, the
    # endpoints stay until the service restarts, and creating them again with
    # the same unique id will fail.
    Write-Host "Removing the loopback" -ForegroundColor Cyan
    Remove-MidiLoopback -AssociationId $associationId

    # Remove-MidiLoopback also takes the association id from the pipeline, so
    # this removes everything created above in one go:
    #     Get-MidiLoopback | Remove-MidiLoopback
}
else
{
    Write-Host "Unable to create the loopback endpoints." -ForegroundColor Red
    Write-Host "This happens if a previous run was killed before removing them." -ForegroundColor Red
    Write-Host "Restart the MIDI service, or change the unique id in this script." -ForegroundColor Red
}

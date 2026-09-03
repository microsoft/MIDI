#Requires -Version 7.6
import-module WindowsMidiServices

# Confirms Windows MIDI Services is available before anything else is attempted.
Start-Midi

# List all available endpoints. Enumeration functions do not require an active session.
Write-Host "Available MIDI Endpoints" -ForegroundColor Cyan
(Get-MidiEndpointDeviceInfo) | Sort-Object -Property Name | Format-Table -AutoSize
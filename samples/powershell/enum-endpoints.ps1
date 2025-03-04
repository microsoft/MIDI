#Requires -Version 7.4
import-module WindowsMidiServices

$sdkinfo = Start-Midi | Write-Output

# List all available endpoints. Enumeration functions do not require an active session.
Write-Host "Available MIDI Endpoints" -ForegroundColor Cyan
(Get-MidiEndpointDeviceInfoList) | Sort-Object -Property Name | Format-Table -AutoSize

# Cleanly shut down the SDK, stop WinRT activation redirection, etc.
Write-Host "Shutting down the SDK" -ForegroundColor Cyan
Stop-Midi
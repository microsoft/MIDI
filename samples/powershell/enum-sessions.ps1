#Requires -Version 7.4
import-module WindowsMidiServices

$sdkinfo = Start-Midi | Write-Output

# list all the active sessions
Write-Host "All active MIDI sessions" -ForegroundColor Cyan
(Get-MidiSessionList) | Sort-Object -Property Name | Format-Table -AutoSize

# Cleanly shut down the SDK, stop WinRT activation redirection, etc.
Write-Host "Shutting down the SDK" -ForegroundColor Cyan
Stop-Midi
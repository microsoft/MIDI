#Requires -Version 7.6
import-module WindowsMidiServices

# Confirms Windows MIDI Services is available before anything else is attempted.
Start-Midi

# list all the active sessions
Write-Host "All active MIDI sessions" -ForegroundColor Cyan
(Get-MidiSession) | Sort-Object -Property Name | Format-Table -AutoSize
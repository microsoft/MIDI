# See this file for service path, name, etc.
. "$PSScriptRoot\Constants.ps1"


Write-Host ""
Write-Host "Stopping service, if present ".PadRight($Padding, '-') -ForegroundColor Yellow
Write-Host ""

sc stop $ServiceName

Write-Host "Deleting service entry, if present ".PadRight($Padding, '-') -ForegroundColor Yellow
Write-Host ""

sc delete $ServiceName

Write-Host ""
Write-Host "Ignore any errors above this line ".PadRight($Padding,'=') -ForegroundColor Magenta
Write-Host ""
Write-Host "Creating service entry ".PadRight($Padding, '-') -ForegroundColor Yellow
Write-Host "At path $Location"
Write-Host ""


sc create $ServiceName binpath="$Location\MidiService.exe"
sc description $ServiceName "In-development version of Windows MIDI Services"

Write-Host ""
Write-Host "Starting service ".PadRight($Padding, '-') -ForegroundColor Yellow
Write-Host ""



sc start $ServiceName

Write-Host "If the above fails, check the event log for runtime errors`n" -ForegroundColor Yellow

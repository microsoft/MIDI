. "$PSScriptRoot\Constants.ps1"

Write-Host ""
Write-Host "List of all MIDI pipes currently open ".PadRight($Padding, '-') -ForegroundColor Yellow
Write-Host ""

[System.IO.Directory]::GetFiles("\\.\\pipe\\", "midi*")

Write-Host ""
Write-Host "End of pipe list ".PadRight($Padding, '-') -ForegroundColor Yellow
Write-Host ""

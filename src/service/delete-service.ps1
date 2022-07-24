# See this file for service path, name, etc.
. "$PSScriptRoot\Constants.ps1"


Write-Host ""
Write-Host "Stopping service, if present ".PadRight($Padding, '-') -ForegroundColor Yellow
Write-Host ""

sc.exe stop $ServiceName

Write-Host "Deleting service entry, if present ".PadRight($Padding, '-') -ForegroundColor Yellow
Write-Host ""

sc.exe delete $ServiceName






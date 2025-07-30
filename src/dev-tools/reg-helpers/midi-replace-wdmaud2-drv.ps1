# Modified by Pete Brown from:
# Developed for PowerShell v4.0
# ** Required Admin privileges **
# Links:
#   http://shrekpoint.blogspot.ru/2012/08/taking-ownership-of-dcom-registry.html
#   http://www.remkoweijnen.nl/blog/2012/01/16/take-ownership-of-a-registry-key-in-powershell/
#   https://powertoe.wordpress.com/2010/08/28/controlling-registry-acl-permissions-with-powershell/


#Requires -RunAsAdministrator
#Requires -Version 7.0

Write-Host
Write-Host "Windows MIDI Services wdmaud2.drv replacement script" -ForegroundColor DarkCyan
Write-Host "=========================================================================" -ForegroundColor DarkGray
Write-Host "This script is for developers and power users who need to run the latest" -ForegroundColor DarkCyan
Write-Host "MIDI 1.0 / WinMM backwards compatibility code on a PC which has Windows" -ForegroundColor DarkCyan
Write-Host "Insider Canary release installed." -ForegroundColor DarkCyan
Write-Host
Write-Host "These actions are not easily reversible without reinstalling Windows." -ForegroundColor DarkCyan
Write-Host

if (!([Environment]::Is64BitProcess))
{
    Write-Host
    Write-Host "This appears to be a 32-bit host. This script must be run from a 64-bit elevated PowerShell 7+ shell." -ForegroundColor Red
    Write-Host
    Exit
}

$confirmation = Read-Host "Do you want to continue? (y/n)"

if ($confirmation -eq 'y' -or $confirmation -eq 'Y')
{
    $wdmaud2SourcePath = ".\wdmaud2.drv"
    $wdmaud2SystemPath = $env:systemroot + "\System32\wdmaud2.drv"
    $wdmaud2SystemBakPath = $env:systemroot + "\System32\wdmaud2.bak"

    if (!Test-Path -Path $wdmaud2SourcePath)
    {
        Write-Host "Expected to find the new wdmaud2.drv in this directory, but it was missing. Cannot proceed." -ForegroundColor Red
        Exit
    }

    if (Test-Path -Path $wdmaud2SystemPath)
    {
        Write-Host "Taking ownership of wdmaud2.drv..." -ForegroundColor DarkCyan
        takeown /F $wdmaud2SystemPath
        icacls $wdmaud2SystemPath /grant administrators:F

        Write-Host "Removing any previous backup file..." -ForegroundColor DarkCyan

        if (Test-Path -Path $wdmaud2SystemBakPath)
        {
            del /F /Q $wdmaud2SystemBakPath
        }

        Write-Host "Renaming existing wdmaud2.drv to wdmaud2.bak..." -ForegroundColor DarkCyan
        Move-Item $wdmaud2SystemPath $wdmaud2SystemBakPath

        # now copy wdmaud2.drv into System32
        Write-Host "Copying over the new wdmaud2.drv..." -ForegroundColor DarkCyan
        xcopy /-I $wdmaud2SourcePath $wdmaud2SystemPath

        Write-Host
        Write-Host "WinMM backwards compatibility support will be in place after you reboot." -ForegroundColor Yellow
    }
    else
    {
        # no existing wdmaud2.drv That shouldn't happen
        Write-Host "No existing wdmaud2.drv found in System32. WinMM will not be able to use Windows MIDI Services." -ForegroundColor Red
    }
}
else
{
    Write-Host
    Write-Host "No actions taken" -ForegroundColor Red
}



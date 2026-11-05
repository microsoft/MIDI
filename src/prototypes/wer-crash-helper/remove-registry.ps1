# PROTOTYPE. Removes everything setup-registry.ps1 added. See README.md. Run elevated.
#
# Removes:
#   - this prototype's WerHelperModule.dll value (any platform or configuration build) from
#     HKLM and HKCU ...\Windows Error Reporting\RuntimeExceptionHelperModules. The keys themselves
#     were already there and hold other products' modules, so they stay.
#   - HKLM ...\Windows Error Reporting\LocalDumps\WerCrashTest.exe
#   - with -RemoveDumps: the per-app dump folder %LOCALAPPDATA%\Microsoft\MIDI\CrashDumps\WerCrashTest
#     (only WerCrashTest.exe.*.dmp, then the folder and its empty parents if nothing else is in
#     them), plus test dumps left in %ProgramData%\Microsoft\MIDI\CrashDumps by the first round,
#     and that folder too if the first round created it and nothing else is in it.
#     Run it from your own elevated session so %LOCALAPPDATA% is his.

param(
    [switch] $RemoveDumps
)

$ErrorActionPreference = 'Stop'

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    throw 'Run this from an elevated PowerShell. LocalDumps lives in HKLM.'
}

$werKey = 'SOFTWARE\Microsoft\Windows\Windows Error Reporting'
$outRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'out')) + '\'

foreach ($hive in [Microsoft.Win32.Registry]::LocalMachine, [Microsoft.Win32.Registry]::CurrentUser) {
    $modules = $hive.OpenSubKey("$werKey\RuntimeExceptionHelperModules", $true)
    if ($null -eq $modules) { continue }

    foreach ($name in $modules.GetValueNames()) {
        if ($name.StartsWith($outRoot, [StringComparison]::OrdinalIgnoreCase) -and $name.EndsWith('\WerHelperModule.dll', [StringComparison]::OrdinalIgnoreCase)) {
            $modules.DeleteValue($name)
            Write-Host "Removed $($hive.Name)\$werKey\RuntimeExceptionHelperModules : $name"
        }
    }

    $modules.Close()
}

$localDumps = [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey("$werKey\LocalDumps", $true)
if ($null -ne $localDumps) {
    if ($localDumps.GetSubKeyNames() -contains 'WerCrashTest.exe') {
        $localDumps.DeleteSubKeyTree('WerCrashTest.exe')
        Write-Host "Removed HKLM\$werKey\LocalDumps\WerCrashTest.exe"
    }

    $localDumps.Close()
}

if ($RemoveDumps) {
    $perApp = Join-Path $env:LOCALAPPDATA 'Microsoft\MIDI\CrashDumps\WerCrashTest'
    if (Test-Path $perApp) {
        Get-ChildItem $perApp -Filter 'WerCrashTest.exe.*.dmp' | ForEach-Object {
            Remove-Item -LiteralPath $_.FullName
            Write-Host "Deleted $($_.FullName)"
        }

        # Remove the folder, then CrashDumps and MIDI above it, each only if now empty.
        $dir = $perApp
        foreach ($i in 1..3) {
            if ((Test-Path $dir) -and @(Get-ChildItem $dir -Force).Count -eq 0) {
                Remove-Item -LiteralPath $dir
                Write-Host "Deleted $dir"
                $dir = Split-Path $dir -Parent
            }
            else {
                break
            }
        }
    }

    $folder = Join-Path $env:ProgramData 'Microsoft\MIDI\CrashDumps'
    if (Test-Path $folder) {
        Get-ChildItem $folder -Filter 'WerCrashTest.exe.*.dmp' | ForEach-Object {
            Remove-Item -LiteralPath $_.FullName
            Write-Host "Deleted $($_.FullName)"
        }

        $marker = Join-Path $folder '.created-by-wer-prototype'
        if ((Test-Path $marker) -and @(Get-ChildItem $folder -Force).Count -eq 1) {
            Remove-Item -LiteralPath $marker -Force
            Remove-Item -LiteralPath $folder
            Write-Host "Deleted $folder"
        }
    }
}

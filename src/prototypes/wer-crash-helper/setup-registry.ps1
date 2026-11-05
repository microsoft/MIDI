# PROTOTYPE. Writes the registry entries the WER crash helper experiment needs. See README.md.
# Run elevated. Undo everything with remove-registry.ps1.
#
# What it writes:
#   1. The module DLL's full path as a DWORD 0 value under
#      <Scope>\SOFTWARE\Microsoft\Windows\Windows Error Reporting\RuntimeExceptionHelperModules
#      (-Scope HKLM, the default, or HKCU to see whether a per-user entry is enough)
#   2. HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\WerCrashTest.exe
#      DumpFolder = %ProgramData%\Microsoft\MIDI\CrashDumps (REG_EXPAND_SZ), DumpType = 2 (full),
#      DumpCount = 10
#   3. The dump folder itself, if it is not there yet
#
# It writes nothing for any other exe.

param(
    [ValidateSet('HKLM', 'HKCU')] [string] $Scope = 'HKLM',
    [ValidateSet('x64', 'ARM64')] [string] $Platform = 'x64',
    [ValidateSet('Release', 'Debug')] [string] $Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    throw 'Run this from an elevated PowerShell. LocalDumps lives in HKLM.'
}

$dll = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "out\$Platform\$Configuration\WerHelperModule.dll"))
if (-not (Test-Path $dll)) {
    throw "Build first: $dll is missing."
}

$werKey = 'SOFTWARE\Microsoft\Windows\Windows Error Reporting'

# 1. Runtime exception module. Value name = full DLL path, value = DWORD 0.
$hive = if ($Scope -eq 'HKLM') { [Microsoft.Win32.Registry]::LocalMachine } else { [Microsoft.Win32.Registry]::CurrentUser }
$modules = $hive.CreateSubKey("$werKey\RuntimeExceptionHelperModules")
$modules.SetValue($dll, 0, [Microsoft.Win32.RegistryValueKind]::DWord)
$modules.Close()
Write-Host "Added $Scope\$werKey\RuntimeExceptionHelperModules : $dll = 0"

# Keep the A/B clean: the module is listed in one hive only.
$other = if ($Scope -eq 'HKLM') { [Microsoft.Win32.Registry]::CurrentUser } else { [Microsoft.Win32.Registry]::LocalMachine }
$otherModules = $other.OpenSubKey("$werKey\RuntimeExceptionHelperModules", $true)
if ($null -ne $otherModules) {
    if ($otherModules.GetValueNames() -contains $dll) {
        $otherModules.DeleteValue($dll)
        Write-Host "Removed $($other.Name)\$werKey\RuntimeExceptionHelperModules : $dll"
    }

    $otherModules.Close()
}

# 2. LocalDumps for the test exe only.
$dumps = [Microsoft.Win32.Registry]::LocalMachine.CreateSubKey("$werKey\LocalDumps\WerCrashTest.exe")
$dumps.SetValue('DumpFolder', '%LOCALAPPDATA%\Microsoft\MIDI\CrashDumps\WerCrashTest', [Microsoft.Win32.RegistryValueKind]::ExpandString)
$dumps.SetValue('DumpType', 2, [Microsoft.Win32.RegistryValueKind]::DWord)
$dumps.SetValue('DumpCount', 10, [Microsoft.Win32.RegistryValueKind]::DWord)
$dumps.Close()
Write-Host "Added HKLM\$werKey\LocalDumps\WerCrashTest.exe (DumpFolder, DumpType=2, DumpCount=10)"

# Modified by Pete Brown from 
# Developed for PowerShell v4.0
# ** Required Admin privileges **
# Links:
#   http://shrekpoint.blogspot.ru/2012/08/taking-ownership-of-dcom-registry.html
#   http://www.remkoweijnen.nl/blog/2012/01/16/take-ownership-of-a-registry-key-in-powershell/
#   https://powertoe.wordpress.com/2010/08/28/controlling-registry-acl-permissions-with-powershell/


#Requires -RunAsAdministrator
#Requires -Version 7.0

function Take-Permissions 
{
    param($rootKey, $key, $comment, [System.Security.Principal.SecurityIdentifier]$sid = 'S-1-5-32-545', $recurse = $true)
    
    switch -regex ($rootKey) 
    {
        'HKCU|HKEY_CURRENT_USER'    { $rootKey = 'CurrentUser' }
        'HKLM|HKEY_LOCAL_MACHINE'   { $rootKey = 'LocalMachine' }
        'HKCR|HKEY_CLASSES_ROOT'    { $rootKey = 'ClassesRoot' }
        'HKCC|HKEY_CURRENT_CONFIG'  { $rootKey = 'CurrentConfig' }
        'HKU|HKEY_USERS'            { $rootKey = 'Users' }
    }

    ### Step 1 - escalate current process's privilege
    # get SeTakeOwnership, SeBackup and SeRestore privileges before executes next lines, script needs Admin privilege
    $import = '[DllImport("ntdll.dll")] public static extern int RtlAdjustPrivilege(ulong a, bool b, bool c, ref bool d);'
    $ntdll = Add-Type -Member $import -Name NtDll -PassThru
    $privileges = @{ SeTakeOwnership = 9; SeBackup =  17; SeRestore = 18 }
    foreach ($i in $privileges.Values) {
        $null = $ntdll::RtlAdjustPrivilege($i, 1, 0, [ref]0)
    }

    function Take-KeyPermissions {
        param($rootKey, $key, $sid, $recurse, $recurseLevel = 0)

        ### Step 2 - get ownerships of key - it works only for current key
        $regKey = [Microsoft.Win32.Registry]::$rootKey.OpenSubKey($key, 'ReadWriteSubTree', 'TakeOwnership')

        if ($regKey)
        {
            $acl = New-Object System.Security.AccessControl.RegistrySecurity
            $acl.SetOwner($sid)
            $regKey.SetAccessControl($acl)

            ### Step 3 - enable inheritance of permissions (not ownership) for current key from parent
            $acl.SetAccessRuleProtection($false, $false)
            $regKey.SetAccessControl($acl)

            ### Step 4 - only for top-level key, change permissions for current key and propagate it for subkeys
            # to enable propagations for subkeys, it needs to execute Steps 2-3 for each subkey (Step 5)
            if ($recurseLevel -eq 0) {
                $regKey = $regKey.OpenSubKey('', 'ReadWriteSubTree', 'ChangePermissions')
                $rule = New-Object System.Security.AccessControl.RegistryAccessRule($sid, 'FullControl', 'ContainerInherit', 'None', 'Allow')
                $acl.ResetAccessRule($rule)
                $regKey.SetAccessControl($acl)
            }

            ### Step 5 - recursively repeat steps 2-5 for subkeys
            if ($recurse) {
                foreach($subKey in $regKey.OpenSubKey('').GetSubKeyNames()) {
                    Take-KeyPermissions $rootKey ($key+'\'+$subKey) $sid $recurse ($recurseLevel+1)
                }
            }

            if ($recurseLevel -eq 0)
            {
                Write-Host "Processed:" -ForegroundColor Green -NoNewline
                Write-Host " " $key -ForegroundColor DarkCyan  -NoNewline
                Write-Host " " $comment -ForegroundColor DarkGray
            }
        }
        else
        {
            Write-Host "Not Found:" -ForegroundColor Red -NoNewline
            Write-Host " " $key -ForegroundColor DarkCyan  -NoNewline
            Write-Host " " $comment -ForegroundColor DarkGray
        }
    }

    Take-KeyPermissions $rootKey $key $sid $recurse

}

Write-Host
Write-Host "Windows MIDI Services Developer Prep Script" -ForegroundColor DarkCyan
Write-Host "=========================================================================" -ForegroundColor DarkGray
Write-Host "This script is for developers and power users who need to run development" -ForegroundColor DarkCyan
Write-Host "builds of Windows MIDI Services on a Windows PC which has the public" -ForegroundColor DarkCyan
Write-Host "release already installed." -ForegroundColor DarkCyan
Write-Host
Write-Host "Running this script will change registry permissions to take ownership of " -ForegroundColor DarkCyan
Write-Host "the component registry entries from Trusted Installer and grant them to you" -ForegroundColor DarkCyan
Write-Host "to enable the Developer Preview installer to install developer bits. This" -ForegroundColor DarkCyan
Write-Host "will also de-register the in-box 'Midisrv' Windows MIDI Service." -ForegroundColor DarkCyan
Write-Host
Write-Host "These actions are not easily reversible without reinstalling Windows." -ForegroundColor DarkCyan
Write-Host

if (!([Environment]::Is64BitProcess))
{
    Write-Host
    Write-Host "This appears to be a 32-bit host. This script must be run from a 64-bit elevated shell." -ForegroundColor Red
    Write-Host
    Exit
}



$confirmation = Read-Host "Do you want to continue? (y/n)"
if ($confirmation -eq 'y' -or $confirmation -eq 'Y')
{
    Write-Host
    Write-Host "Preparing system for Windows MIDI Services development build..." -ForegroundColor Green
    Write-Host

    # Root Windows MIDI Services key. Taking permission of this enables
    # us to add in-development transports
    Take-Permissions "HKLM" "SOFTWARE\Microsoft\Windows MIDI Services" "Main MIDI Services key"

    # The in-box COM registrations need to be unprotected so we can use the ones
    # we install from the setup package rather than what is in System32

    Take-Permissions "HKCR" "CLSID\{0f273b18-e372-4d95-87ac-c31c3d22e937}" "KS Aggregate Transport"
    Take-Permissions "HKCR" "CLSID\{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}" "KS Transport"
    Take-Permissions "HKCR" "CLSID\{942BF02D-93C0-4EA8-B03E-D51156CA75E1}" "Loopback Transport"
    Take-Permissions "HKCR" "CLSID\{8FEAAD91-70E1-4A19-997A-377720A719C1}" "Virtual MIDI Transport"
    Take-Permissions "HKCR" "CLSID\{ac9b5417-3fe0-4e62-960f-034ee4235a1a}" "Diagnostics Transport"

    # --------------------

    # Main MidiSrv Transport (what the client SDK talks to)
    Take-Permissions "HKCR" "CLSID\{2BA15E4E-5417-4A66-85B8-2B2260EFBC84}" "Main Midisrv Transport"

    # --------------------

    Take-Permissions "HKCR" "CLSID\{A8798C54-6066-45F0-9ADB-648BC0641ABF}" "Bytestream 2 UMP Transform"
    Take-Permissions "HKCR" "CLSID\{96121591-8D68-479F-9B48-2BF0B90113F7}" "UMP 2 Bytestream Transform"
    Take-Permissions "HKCR" "CLSID\{a42cde44-7fa9-4597-a8ee-b40b96bcddb1}" "Message Scheduler"
    Take-Permissions "HKCR" "CLSID\{dc638b31-cf31-48ed-9e79-02740bf5d013}" "Protocol Downscaler"

    Write-Host

    if (Get-Process -Name "midisrv" -ErrorAction SilentlyContinue)
    {
        Stop-Process -Name "midisrv" -Force
        Write-Host "Stopped midisrv process" -ForegroundColor DarkCyan
    }
    else
    {
        Write-Host "Midisrv process was not running. No process termination required." -ForegroundColor DarkGray
    }

    if (Get-Service -Name "midisrv" -ErrorAction SilentlyContinue)
    {
        # Finally, we need to be able to replace MidiSrv itself, so remove that entry
        Stop-Service -Name "midisrv"
        Remove-Service -Name "midisrv"

        Write-Host "Removed midisrv service" -ForegroundColor DarkCyan
    }
    else
    {
        Write-Host "Midisrv service was not present. No service action taken." -ForegroundColor DarkGray
    }

    Write-Host
    Write-Host "You may now install the development version of the service and plugins." -ForegroundColor Green

}
else
{
    Write-Host
    Write-Host "No actions taken" -ForegroundColor Red
}



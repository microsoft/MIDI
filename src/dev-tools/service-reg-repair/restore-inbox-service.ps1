#Requires -RunAsAdministrator
#Requires -Version 7.0
    
    function Show-MidiRegInproc32([String] $key, [String] $comment)    
    {
        Write-Host $comment.PadRight(32) -NoNewline -ForegroundColor DarkCyan
        #Write-Host " : " -NoNewline -ForegroundColor White

        $info = Get-ItemProperty -Path $key  -Name "(default)" -ErrorAction SilentlyContinue

        if ($info -eq $null)
        {
             Write-Host "Registration not found" -ForegroundColor Red
        }
        else
        {
            Write-Host $info."(default)"  -ForegroundColor Cyan
        }
    }
    

    Write-Host
    Write-Host "Windows MIDI Services In-Box Service Registration Restore" -ForegroundColor DarkCyan
    Write-Host "".PadRight(78,'=') -ForegroundColor DarkGray
    Write-Host "If you have been using a developer service from Program Files, this will" -ForegroundColor DarkCyan
    Write-Host "restore the service and service plugin registration to use those in System32." -ForegroundColor DarkCyan
    Write-Host "".PadRight(78,'=') -ForegroundColor DarkGray
    Write-Host "Before running this script, you MUST uninstall the developer service package." -ForegroundColor White
    Write-Host "from Settings > Apps > Installed Apps." -ForegroundColor White
    Write-Host "".PadRight(78,'=') -ForegroundColor DarkGray
    Write-Host
    
    # TODO: Need confirmation to continue



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
        $system32Path = [System.Environment]::SystemDirectory

        $service = Get-Service -Name "midisrv" -ErrorAction SilentlyContinue

        $servicePath = $service.BinaryPathName.Trim('"')
        $serviceIsSystem32Already = ($servicePath -like ($system32Path + "*"))

        if ($service -eq $null -or $serviceIsSystem32Already)
        {
            if ($serviceIsSystem32Already)
            {
                Write-Host "midisrv from System32 is already registered as the service. Skipping." -ForegroundColor DarkCyan
            }
            else
            {
                # Restore MIDI Service to the one in System32
                $midisrvPath = Join-Path -Path $system32Path -Child "midisrv.exe"

                Write-Host "Processing " -NoNewline -ForegroundColor DarkCyan
                Write-Host $midisrvPath -ForegroundColor Cyan

                # Because this is not exactly how the service is registered in a Windows update
                # this will still say "Preview" even though it is in System32.
                & $midisrvPath "install"

                Write-Host " -- NOTE: Until the next official update, the service name will still contain 'Preview' even though it is now the System32 in-box version." -ForegroundColor DarkGray
            }

            # Restore plugins
            
            $dllsToRegister =
            @(
                "Midi2.MidiSrvTransport.dll",

                "Midi2.KSAggregateTransport.dll",
                "Midi2.KSTransport.dll",
                "Midi2.LoopbackMidiTransport.dll",
                "Midi2.VirtualMidiTransport.dll",
                "Midi2.DiagnosticsTransport.dll",

                "Midi2.BS2UMPTransform.dll",
                "Midi2.UMP2BSTransform.dll",
                "Midi2.SchedulerTransform.dll",
                "Midi2.UmpProtocolDownscalerTransform.dll"
            )

            foreach ($dll in $dllsToRegister)
            {
                #Write-Host

                $fullDllPath = Join-Path -Path $system32Path -Child $dll

                # validate the file exists
                if (Test-Path -Path $fullDllPath -PathType Leaf)
                {
                    Write-Host "Registering " -NoNewline -ForegroundColor DarkCyan
                    Write-Host $fullDllPath -ForegroundColor Cyan

                    # self-register
                    regsvr32.exe /s "$fullDllPath"
                    $regSvr32ExitCode = $LASTEXITCODE

                    # check if self-registration worked (commented out because we never get the exit code here)
                    #if ($regSvr32ExitCode -eq 0)
                    #{
                    #    Write-Host " -- Registration succeeded." -ForegroundColor DarkGray
                    #}
                    #else
                    #{
                    #    $errorMessage = "$fullDllPath failed to self-register. Exit code: $regSvr32ExitCode"
                    #    Write-Error $errorMessage
                   #}
                }
                else
                {
                    Write-Error ($fullDllPath + " does not exist. Could not self-register.")
                }
            }

        }
        else
        {
            Write-Host
            Write-Error "midisrv service registration found. Did you uninstall the developer service using the Windows Settings app?" -TargetObject $service
            Write-Host "No actions taken" -ForegroundColor Red
            Write-Host
        }
    }
    else
    {
        Write-Host
        Write-Host "No actions taken" -ForegroundColor Red
        Write-Host
    }

    # display info about the MIDI Service that is installed

    Write-Host
    Write-Host "Current State of shipping MIDI Service and Plugins Registration: " -ForegroundColor Cyan
    Write-Host "".PadRight(78,'=') -ForegroundColor DarkGray
    Write-Host
    Write-Host "Windows Service " -ForegroundColor DarkGray

    $service = Get-Service -Name "midisrv" -ErrorAction SilentlyContinue
    
    if ($service -eq $null)
    {
        Write-Host "Midisrv service is not present. " -ForegroundColor DarkGray
    }
    else
    {
        # Restart the service to load the new registrations
        Restart-Service -Name midisrv

        Write-Host $service.DisplayName.PadRight(32) -NoNewline  -ForegroundColor DarkCyan
        Write-Host $service.BinaryPathName -ForegroundColor Cyan
    }

    # Now all the registry entries for COM objects used by the service. This list must be
    # Manually maintained

    Write-Host
    Write-Host "API Interface " -ForegroundColor DarkGray
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{2BA15E4E-5417-4A66-85B8-2B2260EFBC84}\InprocServer32" "Midisrv Transport"

    Write-Host
    Write-Host "In-Box Transports " -ForegroundColor DarkGray
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{0f273b18-e372-4d95-87ac-c31c3d22e937}\InprocServer32" "KS Aggregate (MIDI 1)"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}\InprocServer32" "KS (UMP)"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{942BF02D-93C0-4EA8-B03E-D51156CA75E1}\InprocServer32" "Loopback"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{8FEAAD91-70E1-4A19-997A-377720A719C1}\InprocServer32" "Virtual MIDI"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{ac9b5417-3fe0-4e62-960f-034ee4235a1a}\InprocServer32" "Diagnostics"

    #Write-Host
    #Write-Host "Preview Transports " -ForegroundColor DarkGray
    #Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{c95dcd1f-cde3-4c2d-913c-528cb8a4cbe6}\InprocServer32" "Network MIDI 2.0"

    Write-Host
    Write-Host "In-Box Transforms " -ForegroundColor DarkGray
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{A8798C54-6066-45F0-9ADB-648BC0641ABF}\InprocServer32" "Bytestream 2 UMP"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{96121591-8D68-479F-9B48-2BF0B90113F7}\InprocServer32" "UMP 2 Bytestream"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{a42cde44-7fa9-4597-a8ee-b40b96bcddb1}\InprocServer32" "Message Scheduler"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{dc638b31-cf31-48ed-9e79-02740bf5d013}\InprocServer32" "Protocol Downscaler"


